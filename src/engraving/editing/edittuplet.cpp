/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "edittuplet.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/durationtype.h"
#include "../dom/input.h"
#include "../dom/mscore.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/select.h"

#include "log.h"

using namespace mu::engraving;

// A measure rest has no note value, so it's divided
static bool usesDurationAsNoteValue(const ChordRest* chordRest)
{
    return !chordRest->durationType().isMeasure();
}

static Fraction dividingRatio(const ChordRest* chordRest, const Fraction& ratio, bool autoBaseLen)
{
    Fraction result = ratio;
    if (autoBaseLen) {
        result.setDenominator(Tuplet::computeTupletDenominator(ratio.numerator(), chordRest->ticks()));
    }
    return result;
}

// Selected chords/rests a tuplet can start at, in score order
static std::vector<ChordRest*> selectedTupletChordRests(const Score* score)
{
    // Score order, not pointer order
    const std::set<ChordRest*> selectedChordRests = score->getSelectedChordRests();
    std::vector<ChordRest*> sortedChordRests(selectedChordRests.begin(), selectedChordRests.end());
    std::sort(sortedChordRests.begin(), sortedChordRests.end(), [](const ChordRest* a, const ChordRest* b) {
        return a->tick() != b->tick() ? a->tick() < b->tick() : a->track() < b->track();
    });

    std::vector<ChordRest*> chordRests;

    // Make sure each linked group is handled only once
    std::unordered_set<const LinkedObjects*> handledLinks;

    for (ChordRest* chordRest : sortedChordRests) {
        if (chordRest->links() && !handledLinks.insert(chordRest->links()).second) {
            continue;
        }

        if (!chordRest->isGrace() && !(chordRest->isChord() && toChord(chordRest)->isTrillCueNote())) {
            chordRests.push_back(chordRest);
        }
    }

    return chordRests;
}

static Fraction tupletEndTick(const Score* score, const ChordRest* chordRest, const Fraction& ratio)
{
    if (!usesDurationAsNoteValue(chordRest)) {
        return chordRest->endTick();
    }
    return score->tupletEndTick(chordRest, ratio, chordRest->durationType());
}

// Skip selected chords/rests that the tuplet before them in their track will replace
static std::vector<ChordRest*> tupletStarts(const Score* score, const Fraction& ratio)
{
    std::vector<ChordRest*> starts;
    std::map<track_idx_t, Fraction> tupletEndTicks;
    for (ChordRest* chordRest : selectedTupletChordRests(score)) {
        const auto previous = tupletEndTicks.find(chordRest->track());
        if (previous != tupletEndTicks.end() && chordRest->tick() < previous->second) {
            continue;
        }
        tupletEndTicks[chordRest->track()] = tupletEndTick(score, chordRest, ratio);
        starts.push_back(chordRest);
    }

    return starts;
}

// In a range, a note value tuplet would replace the rest of the range in its track
static bool rangeContinuesAfterNoteValueChordRest(const Score* score)
{
    if (!score->selection().isRange()) {
        return false;
    }

    std::set<track_idx_t> noteValueTracks;
    for (const ChordRest* chordRest : selectedTupletChordRests(score)) {
        if (noteValueTracks.count(chordRest->track())) {
            return true;
        }
        if (usesDurationAsNoteValue(chordRest)) {
            noteValueTracks.insert(chordRest->track());
        }
    }

    return false;
}

static void selectTupletStart(Score* score, const Tuplet* tuplet)
{
    if (tuplet->elements().empty()) {
        return;
    }

    DurationElement* firstElement = tuplet->elements().front();
    // SINGLE also moves the input position
    if (firstElement->isChord()) {
        const std::vector<Note*>& notes = toChord(firstElement)->notes();
        IF_ASSERT_FAILED(!notes.empty()) {
            score->select(firstElement, SelectType::SINGLE);
        } else {
            score->select(notes.front(), SelectType::SINGLE);
            score->select({ notes.begin() + 1, notes.end() }, SelectType::ADD);
        }
    } else {
        score->select(firstElement, SelectType::SINGLE);
    }
}

bool EditTuplet::checkAddTupletsWithSelectedDurations(const Score* score, const Fraction& ratio)
{
    if (rangeContinuesAfterNoteValueChordRest(score)) {
        MScore::setError(MsError::NOTE_VALUE_TUPLET_ON_SEVERAL_CHORD_RESTS);
        return false;
    }

    for (const ChordRest* chordRest : tupletStarts(score, ratio)) {
        if (usesDurationAsNoteValue(chordRest) && !score->canAddTuplet(chordRest, ratio, chordRest->durationType())) {
            MScore::setError(MsError::NO_SPACE_FOR_TUPLET);
            return false;
        }
    }

    return true;
}

std::vector<Tuplet*> EditTuplet::addTuplets(Transaction&, Score* score, const Fraction& ratio, TupletNumberType numberType,
                                            TupletBracketType bracketType, bool autoBaseLen, bool selectedDurationAsNoteValue)
{
    Selection& selection = score->selection();
    const bool wasRangeSelection = selection.isRange();
    const Fraction startTick = selection.tickStart();
    const Fraction endTick = selection.tickEnd();
    const staff_idx_t startStaff = selection.staffStart();
    const staff_idx_t endStaff = selection.staffEnd();
    Box* startBox = selection.startBox();
    Box* endBox = selection.endBox();

    // ticks taken before any tuplet is created
    std::vector<std::pair<ChordRest*, Fraction> > starts;
    for (ChordRest* chordRest : selectedDurationAsNoteValue ? tupletStarts(score, ratio) : selectedTupletChordRests(score)) {
        starts.emplace_back(chordRest, chordRest->tick());
    }

    std::vector<Tuplet*> tuplets;
    // a note value tuplet can end after the range
    Fraction rangeEndTick = endTick;
    for (const auto& [chordRest, tick] : starts) {
        // an earlier tuplet may have replaced it, e.g. on a linked staff
        if (selectedDurationAsNoteValue && score->findCR(tick, chordRest->track()) != chordRest) {
            continue;
        }

        Tuplet* tuplet = nullptr;
        if (selectedDurationAsNoteValue && usesDurationAsNoteValue(chordRest)) {
            tuplet = score->addTuplet(chordRest, ratio, chordRest->durationType(), numberType, bracketType);
            if (tuplet) {
                rangeEndTick = std::max(rangeEndTick, tuplet->endTick());
            }
        } else {
            tuplet = score->addTuplet(chordRest, dividingRatio(chordRest, ratio, autoBaseLen), numberType, bracketType);
        }
        if (tuplet) {
            tuplets.push_back(tuplet);
        }
    }

    if (wasRangeSelection) {
        selection.setRangeTicks(startTick, rangeEndTick, startStaff, endStaff, startBox, endBox);
        selection.updateSelectedElements();
    } else if (!tuplets.empty()) {
        selectTupletStart(score, tuplets.front());
    }

    if (!tuplets.empty()) {
        score->inputState().setDuration(tuplets.front()->baseLen());
    }

    return tuplets;
}
