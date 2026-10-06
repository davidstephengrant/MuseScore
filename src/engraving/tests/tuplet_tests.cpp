/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "engraving/dom/chord.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/input.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/editchord.h"
#include "engraving/editing/editexcerpt.h"
#include "engraving/editing/editmeasurerepeat.h"
#include "engraving/editing/edittie.h"
#include "engraving/editing/edittimesig.h"
#include "engraving/editing/edittuplet.h"
#include "engraving/editing/noteinput.h"
#include "engraving/editing/splitjoinmeasure.h"
#include "engraving/editing/transaction/transaction.h"

#include "utils/scorerw.h"
#include "utils/scorecomp.h"

using namespace mu::engraving;

static const String TUPLET_DATA_DIR(u"tuplet_data/");

class Engraving_TupletTests : public ::testing::Test
{
public:
    struct LongNoteEntry {
        int bar;
        int chordRest;
        Fraction duration;
        std::string rhythm;
        bool rest = false;
    };

    bool createTuplet(int n, ChordRest* cr);
    void tuplet(const char16_t* p1, const char16_t* p2);
    void split(const char16_t* p1, const char16_t* p2);
    void enterLongNotes(const String& file, const std::vector<LongNoteEntry>& entries);
};

bool Engraving_TupletTests::createTuplet(int n, ChordRest* cr)
{
    if (cr->durationType() < TDuration(DurationType::V_128TH)) {
        return false;
    }

    Fraction f(cr->ticks());
    Fraction tick = cr->tick();
    Tuplet* ot    = cr->tuplet();

    f.reduce();         //measure duration might not be reduced
    Fraction ratio(n, f.numerator());
    Fraction fr(1, f.denominator());
    while (ratio.numerator() >= ratio.denominator() * 2) {
        ratio *= Fraction(1, 2);
        fr    *= Fraction(1, 2);
    }

    Tuplet* tuplet = Factory::createTuplet(cr->score()->dummy());
    tuplet->setRatio(ratio);

    //
    // "fr" is the fraction value of one tuple element
    //
    // "tuplet time" is "normal time" / tuplet->ratio()
    //    Example: an 1/8 has 240 midi ticks, in an 1/8 triplet the note
    //             has a tick duration of 240 / (3/2) = 160 ticks
    //             (assume tpq = 480)
    //

    tuplet->setTicks(f);
    TDuration baseLen(fr);
    tuplet->setBaseLen(baseLen);

    tuplet->setTrack(cr->track());
    tuplet->setTick(tick);
    Measure* measure = cr->measure();
    tuplet->setOwnershipParent(measure);

    if (ot) {
        tuplet->setTuplet(ot);
    }
    cr->score()->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    cr->score()->cmdCreateTuplet(cr, tuplet);
    cr->score()->endCmd();
    return true;
}

void Engraving_TupletTests::tuplet(const char16_t* p1, const char16_t* p2)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + p1);
    Measure* m1 = score->firstMeasure();
    ASSERT_TRUE(m1);
    Measure* m2 = m1->nextMeasure();
    ASSERT_TRUE(m2);

    EXPECT_TRUE(m1 != m2);

    Segment* s = m2->first(SegmentType::ChordRest);
    ASSERT_TRUE(s);
    Chord* c = toChord(s->element(0));
    ASSERT_TRUE(c);

    EXPECT_TRUE(createTuplet(3, c));

    EXPECT_TRUE(ScoreComp::saveCompareScore(score, p1, TUPLET_DATA_DIR + p2));
    delete score;
}

TEST_F(Engraving_TupletTests, join1)
{
    tuplet(u"tuplet1.mscx", u"tuplet1-ref.mscx");
}

void Engraving_TupletTests::split(const char16_t* p1, const char16_t* p2)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + p1);
    Measure* m         = score->firstMeasure();
    TimeSig* ts        = Factory::createTimeSig(score->dummy());
    ts->setSig(Fraction(3, 4), TimeSigType::NORMAL);

    score->transactionManager()->transaction(TranslatableString::untranslatable("Engraving tuplet tests"), [&](Transaction& tx) {
        EditData dd(0);
        dd.dropElement = ts;
        dd.modifiers = {};
        dd.dragOffset = QPointF();
        dd.track = 0;
        m->drop(tx, dd);
    });

    EXPECT_TRUE(ScoreComp::saveCompareScore(score, p1, TUPLET_DATA_DIR + p2));
    delete score;
}

TEST_F(Engraving_TupletTests, split1)
{
    split(u"split1.mscx",   u"split1-ref.mscx");
}

TEST_F(Engraving_TupletTests, split2)
{
    split(u"split2.mscx",   u"split2-ref.mscx");
}

TEST_F(Engraving_TupletTests, split3)
{
    split(u"split3.mscx",   u"split3-ref.mscx");
}

TEST_F(Engraving_TupletTests, split4)
{
    split(u"split4.mscx",   u"split4-ref.mscx");
}

//---------------------------------------------------------
//   addStaff
//    Checks that after adding a staff the resulting
//    score is equal to the reference score
//---------------------------------------------------------

TEST_F(Engraving_TupletTests, addStaff)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + "nestedTuplets_addStaff.mscx");
    ASSERT_TRUE(score);

    // add a staff to the existing staff
    // (copied and adapted from void MuseScore::editInstrList() in mscore/instrdialog.cpp)
    Staff* oldStaff   = score->staff(0);
    Staff* newStaff   = Factory::createStaff(oldStaff->part());
    newStaff->setPart(oldStaff->part());
    newStaff->initFromStaffType(oldStaff->staffType(Fraction(0, 1)));
    newStaff->setDefaultClefType(ClefTypeList(ClefType::F));
    KeySigEvent ke = oldStaff->keySigEvent(Fraction(0, 1));
    newStaff->setKey(Fraction(0, 1), ke);
    score->undoInsertStaff(newStaff, 0, true);

    EXPECT_TRUE(ScoreComp::saveCompareScore(score, u"nestedTuplets_addStaff.mscx", TUPLET_DATA_DIR + u"nestedTuplets_addStaff-ref.mscx"));
    delete score;
}

//-----------------------------------------
//    saveLoad
//     checks that properties persist after loading and saving
//-----------------------------------------
TEST_F(Engraving_TupletTests, saveLoad)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + "save-load.mscx");
    ASSERT_TRUE(score);
    //simply load and save
    EXPECT_TRUE(ScoreComp::saveCompareScore(score, u"save-load.mscx", TUPLET_DATA_DIR + u"save-load.mscx"));
    delete score;
}

// the rhythm of a track in a measure, e.g. "3:5[8~ 8 r8] 8. 4": durations as note values
// (r = rest, . = dot, ~ = tie, R = measure rest), tuplets as actual:normal[...]
static std::string rhythm(Measure* measure, track_idx_t track = 0)
{
    std::string result;
    std::vector<const Tuplet*> open;
    for (Segment* s = measure->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        const ChordRest* cr = toChordRest(s->element(track));
        if (!cr) {
            continue;
        }

        std::vector<const Tuplet*> tuplets;
        for (const Tuplet* t = cr->tuplet(); t; t = t->tuplet()) {
            tuplets.insert(tuplets.begin(), t);
        }
        size_t common = 0;
        while (common < open.size() && common < tuplets.size() && open[common] == tuplets[common]) {
            ++common;
        }
        for (; open.size() > common; open.pop_back()) {
            result += "]";
        }
        for (size_t i = common; i < tuplets.size(); ++i) {
            result += " " + std::to_string(tuplets[i]->ratio().numerator()) + ":"
                      + std::to_string(tuplets[i]->ratio().denominator()) + "[";
            open.push_back(tuplets[i]);
        }

        result += (result.empty() || result.back() == '[') ? "" : " ";
        if (cr->durationType().type() == DurationType::V_MEASURE) {
            result += "R";
            continue;
        }
        result += cr->isRest() ? "r" : "";
        result += std::to_string(1 << (static_cast<int>(cr->durationType().type()) - static_cast<int>(DurationType::V_WHOLE)));
        result += std::string(cr->durationType().dots(), '.');
        result += cr->isChord() && toChord(cr)->upNote()->tieFor() ? "~" : "";
    }
    result += std::string(open.size(), ']');
    return result.substr(result.find_first_not_of(' '));
}

//-----------------------------------------
//    longNoteIntoSpaceLeft
//     a note or rest longer than the space left in a tuplet writes
//     all of that space as tuplet notes, even when it needs two values,
//     and continues with ordinary notes or in the next or outer tuplet
//-----------------------------------------

void Engraving_TupletTests::enterLongNotes(const String& file, const std::vector<LongNoteEntry>& entries)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + file + u".mscx");
    ASSERT_TRUE(score);

    for (const LongNoteEntry& e : entries) {
        Measure* m = score->firstMeasure();
        for (int i = 0; i < e.bar; ++i) {
            m = m->nextMeasure();
        }
        Segment* s = m->first(SegmentType::ChordRest);
        for (int i = 0; i < e.chordRest; ++i) {
            s = s->next(SegmentType::ChordRest);
        }
        ASSERT_TRUE(toChordRest(s->element(0))->tuplet());

        score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
        score->setNoteRest(s, 0, e.rest ? NoteVal() : NoteVal(72), e.duration, DirectionV::AUTO);
        score->endCmd();
        EXPECT_EQ(rhythm(m), e.rhythm) << "bar " << e.bar + 1;
    }

    EXPECT_TRUE(score->sanityCheck());
    delete score;
}

TEST_F(Engraving_TupletTests, longNoteIntoSpaceLeft)
{
    enterLongNotes(u"spaceLeft_longNote", {
        // 16th quintuplet: space left 5/16, two values
        { 0, 0, Fraction(1, 2), "5:4[4~ 16~] 8. r16 r2" },
        // eighth quintuplet: space left 5/8, two values
        { 1, 0, Fraction(1, 1), "5:4[2~ 8~] 4. r8" },
        // 16th quintuplet, a rest: space left 5/16, two values
        { 2, 0, Fraction(1, 2), "5:4[r4 r16] r8. r16 r2", true },
        // eighth quintuplet, a rest: space left 5/8, two values
        { 3, 0, Fraction(1, 1), "5:4[r2 r8] r4. r8", true },
        // 16th quintuplet from its 2nd note: space left 4/16
        { 4, 1, Fraction(1, 2), "5:4[r16 4~] 4 r2" },
        // 16th septuplet: space left 7/16
        { 5, 0, Fraction(1, 2), "7:4[4..~] 16 r16 r8 r2" },
        // spills from one eighth triplet into the next
        { 6, 0, Fraction(1, 1), "3:2[4.~] 3:2[4.~] 4 r4" },
        // 16th sextuplet from its 2nd note: space left 5/16
        { 7, 1, Fraction(1, 2), "6:4[r16 4~ 16~] 8. r16 r2" },
        // 9:8 16ths: space left 9/16
        { 8, 0, Fraction(1, 1), "9:8[2~ 16~] 4.. r16" },
        // two 16th quintuplets: two values in each
        { 9, 0, Fraction(1, 1), "5:4[4~ 16~] 5:4[4~ 16~] 4. r8" },
        // 5:3 eighths in 6/8: space left 5/8
        { 10, 0, Fraction(3, 4), "5:3[2~ 8~] 8 r8 r8" },
    });
}

//-----------------------------------------
//    longNoteIntoNestedSpaceLeft
//     the same, from the innermost of three nested tuplets,
//     continuing in the tuplets it is nested in,
//     and replacing a nested tuplet in its way as a whole
//-----------------------------------------

TEST_F(Engraving_TupletTests, longNoteIntoNestedSpaceLeft)
{
    enterLongNotes(u"spaceLeft_nested", {
        // half triplet > eighth quintuplet > 16th septuplet, from its 3rd note
        { 0, 2, Fraction(1, 1), "3:2[ 5:4[ 7:4[r16 r16 4~ 16~] 4.~] 4~ 16 r16 r8 r2]" },
        // the same nested tuplets, a rest
        { 1, 2, Fraction(1, 1), "3:2[ 5:4[ 7:4[r16 r16 r4 r16] r4.] r4 r16 r16 r8 r2]", true },
        // quarter quintuplet > eighth triplet > 32nd sextuplet, from its 2nd note
        { 2, 1, Fraction(1, 1), "5:4[ 3:2[ 6:4[r32 8~ 32~] 4~] 2~ 16. r32 r8 r4]" },
        // quarter quintuplet > two eighth triplets, from the 1st triplet's 3rd note into the 2nd triplet
        { 3, 2, Fraction(1, 2), "5:4[ 3:2[r8 r8 8~] 4. r8 r4 r4]" },
    });
}

//-----------------------------------------
//    nestedTupletsLengthen
//     lengthening a chord inside a tuplet over a nested tuplet
//     replaces the nested tuplet instead of overfilling the measure
//-----------------------------------------
TEST_F(Engraving_TupletTests, nestedTupletsLengthen)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"nestedTuplets_lengthen.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = score->firstMeasure();

    // 9:8 eighth tuplet: three eighths, a nested eighth triplet, four eighths
    ASSERT_EQ(rhythm(m1), "9:8[8 8 8 3:2[8 8 8] 8 8 8 8]");
    Segment* s = m1->first(SegmentType::ChordRest);
    s = s->next(SegmentType::ChordRest)->next(SegmentType::ChordRest);
    ChordRest* third = toChordRest(s->element(0));
    ASSERT_TRUE(third && third->tuplet());

    // a quarter reaches into the nested triplet
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->changeCRlen(third, TDuration(DurationType::V_QUARTER));
    score->endCmd();

    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "9:8[8 8 4 r8 8 8 8 8]");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "9:8[8 8 8 3:2[8 8 8] 8 8 8 8]");
    delete score;
}

TEST_F(Engraving_TupletTests, nestedTupletStartDoesNotSplitTuplet)
{
    // quarter, a quarter triplet starting with an eighth triplet, quarter
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (DurationType duration : { DurationType::V_QUARTER, DurationType::V_HALF, DurationType::V_QUARTER }) {
            is.setDuration(duration);
            NoteVal nval(60);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    Measure* m1 = score->firstMeasure();
    auto crAt = [&](const Fraction& tick) {
        Segment* s = m1->findSegment(SegmentType::ChordRest, tick);
        return s ? toChordRest(s->element(0)) : nullptr;
    };
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->addTuplet(crAt(Fraction(1, 4)), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->addTuplet(crAt(Fraction(1, 4)), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    ASSERT_EQ(rhythm(m1), "4 3:2[ 3:2[8 r8 r8] r4 r4] 4");

    // only the start of both tuplets doesn't split one
    Segment* start = m1->findSegment(SegmentType::ChordRest, Fraction(1, 4));
    EXPECT_FALSE(start->splitsTuplet());
    EXPECT_TRUE(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4) + Fraction(1, 18))->splitsTuplet());
    EXPECT_TRUE(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4) + Fraction(1, 6))->splitsTuplet());

    // Insert adds a quarter before the tuplets
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(start);
    score->select(toChord(crAt(Fraction(1, 4)))->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_QUARTER);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
    });
    EXPECT_EQ(rhythm(m1), "4 4 3:2[ 3:2[8 r8 r8] r4 r4] 4");
    score->undoRedo(true, nullptr);
    ASSERT_EQ(rhythm(m1), "4 3:2[ 3:2[8 r8 r8] r4 r4] 4");

    // the measure splits before the tuplets
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        SplitJoinMeasure::splitMeasure(tx, score, Fraction(1, 4));
    });
    m1 = score->firstMeasure();
    EXPECT_EQ(rhythm(m1), "4");
    EXPECT_EQ(rhythm(m1->nextMeasure()), "3:2[ 3:2[8 r8 r8] r4 r4] 4");
    EXPECT_TRUE(score->sanityCheck());

    delete score;
}

TEST_F(Engraving_TupletTests, insertTimeMovesLaterTuplets)
{
    // voice 1: four quarters; voice 2: an eighth triplet on beat 2
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (int pitch : { 60, 62, 64, 65 }) {
            NoteVal nval(pitch);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    is.setTrack(1);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        NoteVal nval(55);
        Note* note = NoteInput::addPitch(tx, score, nval, false);
        score->addTuplet(note->chord(), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    Measure* m1 = score->firstMeasure();
    ChordRest* first = toChordRest(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4))->element(1));
    ASSERT_TRUE(first && first->tuplet());
    Tuplet* tuplet = first->tuplet();
    ASSERT_EQ(tuplet->tick(), Fraction(1, 4));

    // a quarter inserted on beat 1 moves the triplet to beat 3
    is.setTrack(0);
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_QUARTER);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
    });
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(m1->ticks(), Fraction(5, 4));
    EXPECT_EQ(first->tick(), Fraction(2, 4));
    EXPECT_EQ(tuplet->tick(), Fraction(2, 4));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(tuplet->tick(), Fraction(1, 4));
    score->undoRedo(false, nullptr);
    EXPECT_EQ(tuplet->tick(), Fraction(2, 4));

    // deleting the triplet leaves a rest in its place
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdDeleteTuplet(tuplet, true);
    });
    EXPECT_TRUE(score->sanityCheck());
    Segment* segment = m1->findSegment(SegmentType::ChordRest, Fraction(2, 4));
    ASSERT_TRUE(segment);
    ChordRest* rest = toChordRest(segment->element(1));
    ASSERT_TRUE(rest);
    EXPECT_TRUE(rest->isRest());
    EXPECT_EQ(rest->ticks(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, insertTimeThenDeleteTupletKeepsMeasureComplete)
{
    // four quarters, the second made into an eighth triplet
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (int pitch : { 60, 62, 64, 65 }) {
            NoteVal nval(pitch);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    Measure* m1 = score->firstMeasure();
    Chord* second = toChord(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4))->element(0));
    ASSERT_TRUE(second);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->addTuplet(second, Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    ChordRest* tupletFirst = toChordRest(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4))->element(0));
    ASSERT_TRUE(tupletFirst && tupletFirst->tuplet());
    Tuplet* tuplet = tupletFirst->tuplet();

    // a quarter inserted on beat 1 moves the triplet to beat 3
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_QUARTER);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
    });
    ASSERT_EQ(m1->ticks(), Fraction(5, 4));

    // deleting the triplet leaves a rest on beat 3 and the first quarter on beat 2
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdDeleteTuplet(tuplet, true);
    });
    EXPECT_TRUE(score->sanityCheck());
    ChordRest* beat2 = toChordRest(m1->findSegment(SegmentType::ChordRest, Fraction(1, 4))->element(0));
    ASSERT_TRUE(beat2 && beat2->isChord());
    EXPECT_EQ(toChord(beat2)->upNote()->pitch(), 60);
    Segment* beat3 = m1->findSegment(SegmentType::ChordRest, Fraction(2, 4));
    ASSERT_TRUE(beat3 && beat3->element(0));
    EXPECT_TRUE(beat3->element(0)->isRest());
    EXPECT_EQ(toChordRest(beat3->element(0))->ticks(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, insertMeasureMovesOuterTuplets)
{
    // a quarter triplet made of three eighth triplets, then a half
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_HALF);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (int pitch : { 60, 62 }) {
            NoteVal nval(pitch);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    auto crAt = [&](const Fraction& tick) {
        Segment* s = score->tick2segment(tick, false, SegmentType::ChordRest);
        return s ? toChordRest(s->element(0)) : nullptr;
    };
    for (const Fraction& tick : { Fraction(0, 1), Fraction(0, 1), Fraction(1, 6), Fraction(2, 6) }) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
            score->addTuplet(crAt(tick), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
        });
    }
    ASSERT_TRUE(crAt(Fraction(0, 1))->tuplet());
    Tuplet* outer = crAt(Fraction(0, 1))->tuplet()->tuplet();
    ASSERT_TRUE(outer);
    for (const DurationElement* element : outer->elements()) {
        ASSERT_TRUE(element->isTuplet());
    }

    // a measure inserted before moves the outer triplet along with the inner ones
    score->startCmd(TranslatableString::untranslatable("Tuplet tests"));
    score->insertMeasure(score->firstMeasure());
    score->endCmd();
    EXPECT_EQ(outer->tick(), Fraction(1, 1));

    // deleting the outer triplet leaves a half rest in its place
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdDeleteTuplet(outer, true);
    });
    EXPECT_TRUE(score->sanityCheck());
    ChordRest* rest = crAt(Fraction(1, 1));
    EXPECT_TRUE(rest && rest->isRest() && rest->ticks() == Fraction(1, 2));
    ChordRest* half = crAt(Fraction(3, 2));
    EXPECT_TRUE(half && half->isChord());
    auto insertedMeasureIsRest = [&]() {
        ChordRest* cr = crAt(Fraction(0, 1));
        return cr && cr->isRest() && cr->ticks() == Fraction(1, 1);
    };
    EXPECT_TRUE(insertedMeasureIsRest());

    score->undoRedo(true, nullptr);
    EXPECT_EQ(outer->tick(), Fraction(1, 1));
    EXPECT_TRUE(insertedMeasureIsRest());
    EXPECT_TRUE(score->sanityCheck());
    score->undoRedo(true, nullptr);
    EXPECT_EQ(outer->tick(), Fraction(0, 1));
    EXPECT_TRUE(score->sanityCheck());

    delete score;
}

//-----------------------------------------
//    addTuplet
//     creates tuplets whose total duration differs from the
//     chord/rest they start at, using addTuplet.mscx:
//      m1: eight eighths, m2: half note + half rest,
//      m3: a 9:8 eighth tuplet, m4: eight eighths
//-----------------------------------------

static ChordRest* chordRestAt(Measure* measure, size_t index)
{
    size_t i = 0;
    for (Segment* s = measure->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        if (i++ == index) {
            return toChordRest(s->element(0));
        }
    }
    return nullptr;
}

static Measure* measureAt(Score* score, int index)
{
    Measure* m = score->firstMeasure();
    for (int i = 0; m && i < index; ++i) {
        m = m->nextMeasure();
    }
    return m;
}

static bool addTuplet(ChordRest* cr, const Fraction& ratio, const TDuration& baseLen)
{
    Score* score = cr->score();
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    Tuplet* tuplet = score->addTuplet(cr, ratio, baseLen, TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    score->endCmd();
    return tuplet;
}

TEST_F(Engraving_TupletTests, addTupletLongerThanChordRest)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // 3 eighths in the time of 5, over the first five eighths of m1
    ASSERT_TRUE(addTuplet(chordRestAt(m1, 0), Fraction(3, 5), DurationType::V_EIGHTH));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:5[8 r8 r8] 8 8 8");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");
    delete score;
}

TEST_F(Engraving_TupletTests, addTupletShorterThanChordRest)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m2 = measureAt(score, 1);

    // an eighth triplet on the half note of m2: the remaining quarter becomes a rest
    ASSERT_TRUE(addTuplet(chordRestAt(m2, 0), Fraction(3, 2), DurationType::V_EIGHTH));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m2), "3:2[8 r8 r8] r4 r2");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m2), "2 r2");
    delete score;
}

TEST_F(Engraving_TupletTests, addTupletShorterDotted)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m2 = measureAt(score, 1);

    // 2 dotted sixteenths in the time of 3 on the half note of m2: the tuplet's 9/32 isn't a
    // single note value, and the remaining 7/32 becomes rests
    TDuration dottedSixteenth(DurationType::V_16TH);
    dottedSixteenth.setDots(1);
    ASSERT_TRUE(addTuplet(chordRestAt(m2, 0), Fraction(2, 3), dottedSixteenth));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m2), "2:3[16. r16.] r32 r16 r8 r2");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m2), "2 r2");
    delete score;
}

TEST_F(Engraving_TupletTests, addTupletNested)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    // 3 eighths in the time of 5, over the last five elements of the 9:8 tuplet in m3 (#18980)
    ASSERT_TRUE(addTuplet(chordRestAt(measureAt(score, 2), 4), Fraction(3, 5), DurationType::V_EIGHTH));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 2)), "9:8[8 8 8 8 3:5[8 r8 r8]]");

    // the nested tuplet survives a save and reload
    ASSERT_TRUE(ScoreRW::saveScore(score, u"addTuplet-nested.mscx"));
    delete score;
    score = ScoreRW::readScore(u"addTuplet-nested.mscx", true);
    ASSERT_TRUE(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 2)), "9:8[8 8 8 8 3:5[8 r8 r8]]");
    delete score;
}

TEST_F(Engraving_TupletTests, addTupletKeepsSelection)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);
    Note* note = toChord(chordRestAt(m1, 7))->upNote();
    score->select(note);
    score->inputState().setDuration(DurationType::V_QUARTER);

    // the caller decides what to select, e.g. note input calls selectNewTuplet()
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    Tuplet* tuplet = score->addTuplet(chordRestAt(m1, 0), Fraction(3, 2), TupletNumberType::SHOW_NUMBER,
                                      TupletBracketType::AUTO_BRACKET);
    score->endCmd();
    ASSERT_TRUE(tuplet);
    EXPECT_EQ(rhythm(m1), "3:2[16 r16 r16] 8 8 8 8 8 8 8");
    EXPECT_EQ(score->selection().element(), note);
    EXPECT_EQ(score->inputState().duration(), TDuration(DurationType::V_QUARTER));
    delete score;
}

// Divides the selected chords/rests, as the tuplet commands do
static std::vector<Tuplet*> addTuplets(Score* score, const Fraction& ratio)
{
    std::vector<Tuplet*> tuplets;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Engraving tuplet tests"), [&](Transaction& tx) {
        tuplets = EditTuplet::addTuplets(tx, score, ratio, TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET,
                                         /*autoBaseLen*/ true);
    });
    return tuplets;
}

// Adds a second staff to the first staff's part, linked to it or with measure rests
static void addSecondStaff(Score* score, bool linked)
{
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    Staff* staff = score->staff(0);
    Staff* newStaff = Factory::createStaff(staff->part());
    newStaff->setPart(staff->part());
    score->undoInsertStaff(newStaff, 1, /*createRests*/ !linked);
    if (linked) {
        Excerpt::cloneStaff(staff, newStaff);
    }
    score->endCmd();
}

TEST_F(Engraving_TupletTests, addTuplets)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // 3 eighths, selected from last to first: the tuplets come back in score order
    score->select(toChord(chordRestAt(m1, 5))->upNote(), SelectType::SINGLE);
    score->select(toChord(chordRestAt(m1, 2))->upNote(), SelectType::ADD);
    score->select(toChord(chordRestAt(m1, 0))->upNote(), SelectType::ADD);
    const std::vector<Tuplet*> tuplets = addTuplets(score, Fraction(3, 2));
    ASSERT_EQ(tuplets.size(), 3);
    EXPECT_EQ(tuplets[0]->tick(), Fraction(0, 1));
    EXPECT_EQ(tuplets[1]->tick(), Fraction(1, 4));
    EXPECT_EQ(tuplets[2]->tick(), Fraction(5, 8));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[16 r16 r16] 8 3:2[16 r16 r16] 8 8 3:2[16 r16 r16] 8 8");

    // the start of the first tuplet is selected, and its note value becomes the input duration
    EXPECT_TRUE(score->selection().isSingle());
    EXPECT_EQ(score->selection().element(), toChord(tuplets[0]->elements().front())->upNote());
    EXPECT_EQ(score->inputState().duration(), TDuration(DurationType::V_16TH));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsKeepsRange)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(m1, 1), SelectType::RANGE);
    ASSERT_EQ(score->getSelectedChordRests().size(), 2);
    EXPECT_EQ(addTuplets(score, Fraction(3, 2)).size(), 2);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[16 r16 r16] 3:2[16 r16 r16] 8 8 8 8 8 8");

    const Selection& selection = score->selection();
    EXPECT_TRUE(selection.isRange());
    EXPECT_EQ(selection.tickStart(), Fraction(0, 1));
    EXPECT_EQ(selection.tickEnd(), Fraction(1, 4));
    EXPECT_EQ(selection.staffStart(), 0);
    EXPECT_EQ(selection.staffEnd(), 1);
    EXPECT_EQ(score->getSelectedChordRests().size(), 6);

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsRangeAcrossStaves)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    addSecondStaff(score, /*linked*/ false);
    Measure* m1 = measureAt(score, 0);

    // the first 2 eighths of m1 and the measure rest below them
    score->selection().setRange(chordRestAt(m1, 0)->segment(), chordRestAt(m1, 2)->segment(), 0, 2);
    score->selection().updateSelectedElements();
    ASSERT_EQ(score->getSelectedChordRests().size(), 3);
    const std::vector<Tuplet*> tuplets = addTuplets(score, Fraction(3, 2));
    EXPECT_TRUE(score->sanityCheck());

    // score order: by tick, then by track
    ASSERT_EQ(tuplets.size(), 3);
    EXPECT_EQ(tuplets[0]->tick(), Fraction(0, 1));
    EXPECT_EQ(tuplets[0]->staffIdx(), 0);
    EXPECT_EQ(tuplets[1]->tick(), Fraction(0, 1));
    EXPECT_EQ(tuplets[1]->staffIdx(), 1);
    EXPECT_EQ(tuplets[2]->tick(), Fraction(1, 8));
    EXPECT_EQ(tuplets[2]->staffIdx(), 0);

    const Selection& selection = score->selection();
    EXPECT_TRUE(selection.isRange());
    EXPECT_EQ(selection.tickStart(), Fraction(0, 1));
    EXPECT_EQ(selection.tickEnd(), Fraction(1, 4));
    EXPECT_EQ(selection.staffStart(), 0);
    EXPECT_EQ(selection.staffEnd(), 2);

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsLinkedStaff)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    addSecondStaff(score, /*linked*/ true);
    Measure* m1 = measureAt(score, 0);

    // the first eighth of m1 on both staves: one tuplet, which the linked staff gets too
    Segment* first = m1->first(SegmentType::ChordRest);
    score->select(first->cr(0), SelectType::RANGE);
    score->select(first->cr(VOICES), SelectType::RANGE);
    ASSERT_EQ(score->getSelectedChordRests().size(), 2);
    EXPECT_EQ(addTuplets(score, Fraction(3, 2)).size(), 1);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[16 r16 r16] 8 8 8 8 8 8 8");

    const Tuplet* linkedTuplet = first->cr(VOICES)->tuplet();
    ASSERT_TRUE(linkedTuplet);
    EXPECT_EQ(linkedTuplet->ratio(), Fraction(3, 2));
    EXPECT_FALSE(linkedTuplet->tuplet());
    EXPECT_EQ(linkedTuplet->elements().size(), 3);
    EXPECT_TRUE(score->selection().isRange());
    EXPECT_EQ(score->selection().staffEnd(), 2);

    delete score;
}

TEST_F(Engraving_TupletTests, canAddTuplet)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    const TDuration eighth(DurationType::V_EIGHTH);

    // fits exactly up to the barline
    EXPECT_TRUE(score->canAddTuplet(chordRestAt(measureAt(score, 3), 3), Fraction(3, 5), eighth));
    // would cross the barline
    EXPECT_FALSE(score->canAddTuplet(chordRestAt(measureAt(score, 3), 4), Fraction(3, 5), eighth));
    // fits exactly up to the end of the parent tuplet
    EXPECT_TRUE(score->canAddTuplet(chordRestAt(measureAt(score, 2), 4), Fraction(3, 5), eighth));
    // would cross the end of the parent tuplet
    EXPECT_FALSE(score->canAddTuplet(chordRestAt(measureAt(score, 2), 5), Fraction(3, 5), eighth));
    // invalid ratio
    EXPECT_FALSE(score->canAddTuplet(chordRestAt(measureAt(score, 0), 0), Fraction(0, 5), eighth));

    delete score;
}

TEST_F(Engraving_TupletTests, canAddTupletInEmptyVoice)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    // halfway through m1, where voice 2 is empty
    const Segment* segment = chordRestAt(measureAt(score, 0), 4)->segment();
    ASSERT_FALSE(segment->cr(1));

    // fits exactly up to the barline
    EXPECT_TRUE(score->canAddTuplet(segment, 1, Fraction(3, 2), DurationType::V_QUARTER));
    // would cross the barline
    EXPECT_FALSE(score->canAddTuplet(segment, 1, Fraction(3, 2), DurationType::V_HALF));
    // with a chord/rest in the track, the same as for that chord/rest: would cross the barline
    EXPECT_FALSE(score->canAddTuplet(chordRestAt(measureAt(score, 3), 4)->segment(), 0, Fraction(3, 5), DurationType::V_EIGHTH));
    // invalid ratio
    EXPECT_FALSE(score->canAddTuplet(segment, 1, Fraction(0, 2), DurationType::V_QUARTER));
    // no track
    EXPECT_FALSE(score->canAddTuplet(segment, muse::nidx, Fraction(3, 2), DurationType::V_QUARTER));

    delete score;
}

TEST_F(Engraving_TupletTests, canAddTupletInEmptyVoiceDotted)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    TDuration dottedEighth(DurationType::V_EIGHTH);
    dottedEighth.setDots(1);
    TDuration doubleDottedEighth(DurationType::V_EIGHTH);
    doubleDottedEighth.setDots(2);

    // halfway through m1, where voice 2 is empty
    const Segment* segment = chordRestAt(measureAt(score, 0), 4)->segment();
    // 3 dotted eighths in the time of 2: 3/8 fits
    EXPECT_TRUE(score->canAddTuplet(segment, 1, Fraction(3, 2), dottedEighth));
    // 2 dotted eighths in the time of 3: 9/16 would cross the barline
    EXPECT_FALSE(score->canAddTuplet(segment, 1, Fraction(2, 3), dottedEighth));
    // 3 double-dotted eighths in the time of 2: 7/16 fits
    EXPECT_TRUE(score->canAddTuplet(segment, 1, Fraction(3, 2), doubleDottedEighth));
    // 2 double-dotted eighths in the time of 3: 21/32 would cross the barline
    EXPECT_FALSE(score->canAddTuplet(segment, 1, Fraction(2, 3), doubleDottedEighth));

    delete score;
}

TEST_F(Engraving_TupletTests, canAddTupletInsideLongerChordRest)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // voice 2 of m1: 3 quarter rests in the time of 2 over the first half
    Segment* first = m1->first(SegmentType::ChordRest);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->expandVoice(first, 1);
    score->endCmd();
    ASSERT_TRUE(first->cr(1));
    ASSERT_TRUE(addTuplet(first->cr(1), Fraction(3, 2), DurationType::V_QUARTER));

    // the second eighth of m1, inside the first rest of that tuplet
    const Segment* segment = m1->findSegment(SegmentType::ChordRest, Fraction(1, 8));
    ASSERT_TRUE(segment);
    ASSERT_FALSE(segment->cr(1));

    // 3 eighths in the time of 2 fit in the tuplet
    EXPECT_TRUE(score->canAddTuplet(segment, 1, Fraction(3, 2), DurationType::V_EIGHTH));
    // 4 quarters in the time of 3 would cross the end of the tuplet, though not the barline
    EXPECT_FALSE(score->canAddTuplet(segment, 1, Fraction(4, 3), DurationType::V_QUARTER));

    delete score;
}

TEST_F(Engraving_TupletTests, tupletEndTick)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    const TDuration eighth(DurationType::V_EIGHTH);

    // 3 eighths in the time of 2, from the start of m1
    EXPECT_EQ(score->tupletEndTick(chordRestAt(measureAt(score, 0), 0), Fraction(3, 2), eighth), Fraction(1, 4));
    // 3 eighths in the time of 5, from halfway through m1: it ends after the barline
    EXPECT_EQ(score->tupletEndTick(chordRestAt(measureAt(score, 0), 4), Fraction(3, 5), eighth), Fraction(9, 8));
    // 3 eighths in the time of 5 in the 9:8 tuplet of m3: it ends with the 9:8 tuplet
    EXPECT_EQ(score->tupletEndTick(chordRestAt(measureAt(score, 2), 4), Fraction(3, 5), eighth), measureAt(score, 2)->endTick());

    delete score;
}

TEST_F(Engraving_TupletTests, tupletEndTickDotted)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);

    TDuration dottedEighth(DurationType::V_EIGHTH);
    dottedEighth.setDots(1);
    TDuration doubleDottedEighth(DurationType::V_EIGHTH);
    doubleDottedEighth.setDots(2);

    // from the start of m1
    ChordRest* first = chordRestAt(measureAt(score, 0), 0);
    // 2 dotted eighths in the time of 3: 9/16
    EXPECT_EQ(score->tupletEndTick(first, Fraction(2, 3), dottedEighth), Fraction(9, 16));
    // 2 double-dotted eighths in the time of 3: 21/32
    EXPECT_EQ(score->tupletEndTick(first, Fraction(2, 3), doubleDottedEighth), Fraction(21, 32));

    delete score;
}

// Checks and adds tuplets that use the selected durations as the subdivision, as the tuplet commands do
static std::vector<Tuplet*> addTupletsWithSelectedDurations(Score* score, const Fraction& ratio)
{
    MScore::setError(MsError::MS_NO_ERROR);
    if (!EditTuplet::checkAddTupletsWithSelectedDurations(score, ratio)) {
        return {};
    }

    std::vector<Tuplet*> tuplets;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Engraving tuplet tests"), [&](Transaction& tx) {
        tuplets = EditTuplet::addTuplets(tx, score, ratio, TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET,
                                         /*autoBaseLen*/ true, TupletSelectedDuration::SUBDIVISION);
    });
    return tuplets;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurations)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // the first 3 eighths of m1: the first tuplet replaces the second eighth, so it gets no tuplet of its own
    score->select(chordRestAt(m1, 0), SelectType::SINGLE);
    score->select(chordRestAt(m1, 1), SelectType::ADD);
    score->select(chordRestAt(m1, 2), SelectType::ADD);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 2);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 3:2[8 r8 r8] 8 8 8 8");

    // the start of the first tuplet is selected
    EXPECT_EQ(score->selection().element(), toChord(chordRestAt(m1, 0))->upNote());
    EXPECT_EQ(score->inputState().duration(), TDuration(DurationType::V_EIGHTH));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsInRange)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // a range of 1 eighth
    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 1);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 8 8 8 8 8 8");

    // the range grows to the end of the tuplet
    EXPECT_TRUE(score->selection().isRange());
    EXPECT_EQ(score->selection().tickStart(), Fraction(0, 1));
    EXPECT_EQ(score->selection().tickEnd(), Fraction(1, 4));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");

    // a range of 3 eighths: the first tuplet replaces the second eighth, and the third gets a tuplet that ends after
    // the range
    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(m1, 2), SelectType::RANGE);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 2);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 3:2[8 r8 r8] 8 8 8 8");
    EXPECT_EQ(score->selection().tickEnd(), Fraction(1, 2));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");

    // the whole measure: a tuplet on every other eighth
    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(m1, 7), SelectType::RANGE);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 4);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 3:2[8 r8 r8] 3:2[8 r8 r8] 3:2[8 r8 r8]");

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsNoSpace)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // the last eighth's tuplet would cross the barline: nothing fits
    score->select(chordRestAt(m1, 7), SelectType::SINGLE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(3, 2)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);
    EXPECT_EQ(rhythm(m1), "8 8 8 8 8 8 8 8");

    // the first one fits, the last one is left out
    score->select(chordRestAt(m1, 0), SelectType::SINGLE);
    score->select(chordRestAt(m1, 7), SelectType::ADD);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 1);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 8 8 8 8 8 8");

    score->undoRedo(true, nullptr);

    // would cross the end of the 9:8 tuplet of m3
    Measure* m3 = measureAt(score, 2);
    score->select(chordRestAt(m3, 8), SelectType::SINGLE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(3, 2)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsInRangeNoSpace)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    TDuration dottedHalf(DurationType::V_HALF);
    dottedHalf.setDots(1);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->changeCRlen(chordRestAt(m1, 0), dottedHalf);
    score->endCmd();
    ASSERT_EQ(rhythm(m1), "2. 8 8");

    // 3 dotted halves in the time of 2 don't fit, so they replace nothing: the next eighth gets its tuplet
    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(m1, 2), SelectType::RANGE);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 1);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "2. 3:2[8 r8 r8]");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "2. 8 8");

    // a dotted half and a quarter: neither tuplet fits
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->changeCRlen(chordRestAt(m1, 1), TDuration(DurationType::V_QUARTER));
    score->endCmd();
    ASSERT_EQ(rhythm(m1), "2. 4");
    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(m1, 1), SelectType::RANGE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(3, 2)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);
    EXPECT_EQ(rhythm(m1), "2. 4");

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsRangeAcrossStaves)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    addSecondStaff(score, /*linked*/ false);
    Measure* m1 = measureAt(score, 0);

    // m1 on both staves: the eighths of the first staff get a tuplet on every other eighth, and the measure rest of the
    // second staff is left as it is
    score->selection().setRange(chordRestAt(m1, 0)->segment(), chordRestAt(measureAt(score, 1), 0)->segment(), 0, 2);
    score->selection().updateSelectedElements();
    const std::vector<Tuplet*> tuplets = addTupletsWithSelectedDurations(score, Fraction(3, 2));
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 3:2[8 r8 r8] 3:2[8 r8 r8] 3:2[8 r8 r8]");
    EXPECT_TRUE(m1->first(SegmentType::ChordRest)->cr(VOICES)->durationType().isMeasure());

    ASSERT_EQ(tuplets.size(), 4);
    for (const Tuplet* tuplet : tuplets) {
        EXPECT_EQ(tuplet->staffIdx(), 0);
    }

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsMeasureRest)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    // a measure rest has no duration to use as the subdivision, so it gets no tuplet, alone or in a range
    score->select(chordRestAt(m1, 0), SelectType::SINGLE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(3, 2)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);

    score->select(chordRestAt(m1, 0), SelectType::RANGE);
    score->select(chordRestAt(measureAt(score, 1), 0), SelectType::RANGE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(3, 2)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);
    EXPECT_EQ(rhythm(m1), "R");
    EXPECT_EQ(rhythm(measureAt(score, 1)), "R");

    // with the selected duration as the tuplet length, each is divided
    EXPECT_EQ(addTuplets(score, Fraction(3, 2)).size(), 2);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[r2 r2 r2]");
    EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[r2 r2 r2]");

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsLinkedStaff)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    addSecondStaff(score, /*linked*/ true);
    Measure* m1 = measureAt(score, 0);

    // the first eighth of m1 on both staves: the tuplet on the first staff replaces the linked one
    Segment* first = m1->first(SegmentType::ChordRest);
    score->select(first->cr(0), SelectType::RANGE);
    score->select(first->cr(VOICES), SelectType::RANGE);
    ASSERT_EQ(score->getSelectedChordRests().size(), 2);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 1);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[8 r8 r8] 8 8 8 8 8 8");

    const Tuplet* linkedTuplet = first->cr(VOICES)->tuplet();
    ASSERT_TRUE(linkedTuplet);
    EXPECT_EQ(linkedTuplet->ratio(), Fraction(3, 2));
    EXPECT_FALSE(linkedTuplet->tuplet());
    EXPECT_EQ(linkedTuplet->elements().size(), 3);

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsDotted)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = measureAt(score, 0);

    TDuration dottedQuarter(DurationType::V_QUARTER);
    dottedQuarter.setDots(1);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->changeCRlen(chordRestAt(m1, 0), dottedQuarter);
    score->endCmd();
    ASSERT_EQ(rhythm(m1), "4. 8 8 8 8 8");

    // 3 dotted quarters in the time of 2 end at 3/4: the eighth at 3/8 gets no tuplet of its own
    score->select(chordRestAt(m1, 0), SelectType::SINGLE);
    score->select(chordRestAt(m1, 1), SelectType::ADD);
    score->select(chordRestAt(m1, 4), SelectType::ADD);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 2);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "3:2[4. r4. r4.] 3:2[8 r8 r8]");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m1), "4. 8 8 8 8 8");

    // 2 dotted quarters in the time of 3: 9/8 would cross the barline
    score->select(chordRestAt(m1, 0), SelectType::SINGLE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(2, 3)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);

    delete score;
}

TEST_F(Engraving_TupletTests, addTupletsWithSelectedDurationsDoubleDotted)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"addTuplet.mscx");
    ASSERT_TRUE(score);
    Measure* m2 = measureAt(score, 1);

    TDuration doubleDottedQuarter(DurationType::V_QUARTER);
    doubleDottedQuarter.setDots(2);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->changeCRlen(chordRestAt(m2, 0), doubleDottedQuarter);
    score->endCmd();
    ASSERT_EQ(rhythm(m2), "4.. r16 r2");

    // 3 double-dotted quarters in the time of 2 end at 7/8: the 16th rest at 7/16 gets no tuplet of its own
    score->select(chordRestAt(m2, 0), SelectType::SINGLE);
    score->select(chordRestAt(m2, 1), SelectType::ADD);
    EXPECT_EQ(addTupletsWithSelectedDurations(score, Fraction(3, 2)).size(), 1);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m2), "3:2[4.. r4.. r4..] r8");

    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(m2), "4.. r16 r2");

    // 2 double-dotted quarters in the time of 3: 21/16 would cross the barline
    score->select(chordRestAt(m2, 0), SelectType::SINGLE);
    EXPECT_TRUE(addTupletsWithSelectedDurations(score, Fraction(2, 3)).empty());
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);

    delete score;
}

TEST_F(Engraving_TupletTests, conventionalTupletDenominator)
{
    // duplet to nonuplet: 2:3, 3:2, 4:3, 5:4, 6:4, 7:4, 8:6, 9:8
    const std::vector<std::pair<int, int> > ratios = { { 2, 3 }, { 3, 2 }, { 4, 3 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 8, 6 }, { 9, 8 } };
    for (const auto& [numerator, denominator] : ratios) {
        EXPECT_EQ(Tuplet::conventionalTupletDenominator(numerator), denominator) << numerator;
    }
    EXPECT_EQ(Tuplet::conventionalTupletDenominator(0), 0);
}

//-----------------------------------------
//    deleteTupletSelection
//     deleting a tuplet selects the rest that replaces it
//-----------------------------------------
TEST_F(Engraving_TupletTests, deleteTupletSelection)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tuplet1.mscx");
    ASSERT_TRUE(score);
    Measure* m2 = score->firstMeasure()->nextMeasure();
    ASSERT_TRUE(m2 && m2 != score->lastMeasure());

    // a triplet from the first chord of bar 2
    ChordRest* first = m2->first(SegmentType::ChordRest)->cr(0);
    ASSERT_TRUE(first && first->isChord());
    ASSERT_TRUE(createTuplet(3, first));
    Tuplet* tuplet = m2->first(SegmentType::ChordRest)->cr(0)->tuplet();
    ASSERT_TRUE(tuplet);
    Fraction tupletTick = tuplet->tick();

    score->select(tuplet);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->cmdDeleteSelection();
    score->endCmd();

    ChordRest* selected = score->selection().firstChordRest();
    ASSERT_TRUE(selected);
    EXPECT_TRUE(selected->isRest());
    EXPECT_EQ(selected->measure(), m2);
    EXPECT_EQ(selected->tick(), tupletTick);
    EXPECT_FALSE(selected->tuplet());
    delete score;
}

//-----------------------------------------
//    deleteNestedTupletSelection
//     deleting a nested tuplet selects the rest that replaces it
//     in the outer tuplet
//-----------------------------------------
TEST_F(Engraving_TupletTests, deleteNestedTupletSelection)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"nestedTuplets_lengthen.mscx");
    ASSERT_TRUE(score);
    Measure* m1 = score->firstMeasure();
    ASSERT_EQ(rhythm(m1), "9:8[8 8 8 3:2[8 8 8] 8 8 8 8]");

    // the nested triplet starts on the 4th chord
    Segment* s = m1->first(SegmentType::ChordRest);
    for (int i = 0; i < 3; ++i) {
        s = s->next(SegmentType::ChordRest);
    }
    Tuplet* nested = s->cr(0)->tuplet();
    ASSERT_TRUE(nested && nested->tuplet());
    Tuplet* outer = nested->tuplet();
    Fraction nestedTick = nested->tick();

    score->select(nested);
    score->startCmd(TranslatableString::untranslatable("Engraving tuplet tests"));
    score->cmdDeleteSelection();
    score->endCmd();

    EXPECT_EQ(rhythm(m1), "9:8[8 8 8 r4 8 8 8 8]");
    ChordRest* selected = score->selection().firstChordRest();
    ASSERT_TRUE(selected);
    EXPECT_TRUE(selected->isRest());
    EXPECT_EQ(selected->tick(), nestedTick);
    EXPECT_EQ(selected->tuplet(), outer);
    delete score;
}

TEST_F(Engraving_TupletTests, removeTimeMovesLaterTuplets)
{
    // four quarters, the second made into an eighth triplet
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (int pitch : { 60, 62, 64, 65 }) {
            NoteVal nval(pitch);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    Measure* m1 = score->firstMeasure();
    auto crAt = [&](const Fraction& tick) {
        Segment* s = m1->findSegment(SegmentType::ChordRest, tick);
        return s ? toChordRest(s->element(0)) : nullptr;
    };
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->addTuplet(crAt(Fraction(1, 4)), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    Tuplet* tuplet = crAt(Fraction(1, 4))->tuplet();
    ASSERT_TRUE(tuplet);

    // removing the first quarter moves the triplet to beat 1
    score->select(toChord(crAt(Fraction(0, 1)))->upNote());
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdTimeDelete();
    });
    EXPECT_TRUE(score->sanityCheck());
    ASSERT_EQ(rhythm(m1), "3:2[8 r8 r8] 4 4");
    EXPECT_EQ(tuplet->tick(), Fraction(0, 1));

    score->undoRedo(true, nullptr);
    EXPECT_EQ(tuplet->tick(), Fraction(1, 4));
    score->undoRedo(false, nullptr);
    EXPECT_EQ(tuplet->tick(), Fraction(0, 1));

    // deleting the triplet leaves a rest in its place
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdDeleteTuplet(tuplet, true);
    });
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "r4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, removeInsertedTimeMovesNestedTupletsBack)
{
    // quarter, a quarter triplet starting with an eighth triplet, quarter
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        for (DurationType duration : { DurationType::V_QUARTER, DurationType::V_HALF, DurationType::V_QUARTER }) {
            is.setDuration(duration);
            NoteVal nval(60);
            NoteInput::addPitch(tx, score, nval, false);
        }
    });
    Measure* m1 = score->firstMeasure();
    auto crAt = [&](const Fraction& tick) {
        Segment* s = m1->findSegment(SegmentType::ChordRest, tick);
        return s ? toChordRest(s->element(0)) : nullptr;
    };
    for (int i = 0; i < 2; ++i) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
            score->addTuplet(crAt(Fraction(1, 4)), Fraction(3, 2), TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
        });
    }
    ASSERT_EQ(rhythm(m1), "4 3:2[ 3:2[8 r8 r8] r4 r4] 4");
    Tuplet* inner = crAt(Fraction(1, 4))->tuplet();
    ASSERT_TRUE(inner && inner->tuplet());
    Tuplet* outer = inner->tuplet();

    // Insert adds a quarter on beat 1, then removing it moves both tuplets back to beat 2
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(m1->first(SegmentType::ChordRest));
    score->select(toChord(crAt(Fraction(0, 1)))->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_QUARTER);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction& tx) {
        NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
    });
    ASSERT_EQ(rhythm(m1), "4 4 3:2[ 3:2[8 r8 r8] r4 r4] 4");
    ASSERT_EQ(outer->tick(), Fraction(2, 4));
    is.setNoteEntryMode(false);
    score->select(toChord(crAt(Fraction(0, 1)))->upNote());
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdTimeDelete();
    });
    EXPECT_TRUE(score->sanityCheck());
    ASSERT_EQ(rhythm(m1), "4 3:2[ 3:2[8 r8 r8] r4 r4] 4");
    EXPECT_EQ(outer->tick(), Fraction(1, 4));
    EXPECT_EQ(inner->tick(), Fraction(1, 4));

    // deleting the outer triplet leaves a rest in its place
    score->transactionManager()->transaction(TranslatableString::untranslatable("Tuplet tests"), [&](Transaction&) {
        score->cmdDeleteTuplet(outer, true);
    });
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(m1), "4 r4 r4 4");

    delete score;
}

//-----------------------------------------
//    pending tuplets
//     in By duration and Rhythm input, a tuplet waits for the
//     next duration, in an empty 4/4 score
//-----------------------------------------

static MasterScore* pendingTupletScore(NoteEntryMethod method = NoteEntryMethod::BY_DURATION)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNotes({ NoteVal(60) });
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(method);
    return score;
}

// like a triplet command in By duration or Rhythm input
static void setPendingTriplet(Score* score, bool durationAsSubdivision)
{
    InputState& is = score->inputState();
    PendingTuplet pending;
    pending.ratio = Fraction(3, 2);
    pending.numberType = TupletNumberType::SHOW_NUMBER;
    pending.bracketType = TupletBracketType::AUTO_BRACKET;
    pending.autoBaseLen = true;
    pending.durationAsSubdivision = durationAsSubdivision;
    pending.tick = is.tick();
    pending.track = is.track();
    pending.beyondScore = is.beyondScore();
    is.setPendingTuplet(pending);
}

static void pressDuration(Score* score, DurationType duration)
{
    // as after each command in the app; a refused duration leaves an error, which would roll back the next one
    MScore::setError(MsError::MS_NO_ERROR);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        NoteInput::setDuration(tx, score, duration);
    });
}

TEST_F(Engraving_TupletTests, pendingTupletNoteValue)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // triplet, eighth: an eighth triplet with the note first
        setPendingTriplet(score, true);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
        EXPECT_FALSE(is.pendingTuplet());
        EXPECT_EQ(is.tick(), Fraction(1, 12));
        EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] r4 r2");

        // undoing the first note takes the tuplet away too, and the tuplet waits again
        score->undoRedo(true, nullptr);
        score->undoRedo(true, nullptr);
        score->undoRedo(true, nullptr);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        EXPECT_TRUE(is.pendingTupletHere());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletUndoAfterEscape)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // after Escape, undoing the first note doesn't bring the tuplet back
        setPendingTriplet(score, true);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        is.forgetPendingTuplets();
        for (int i = 0; i < 3; ++i) {
            score->undoRedo(true, nullptr);
        }
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        EXPECT_FALSE(is.pendingTuplet());

        // a tuplet that waits after Escape comes back
        setPendingTriplet(score, true);
        pressDuration(score, DurationType::V_EIGHTH);
        score->undoRedo(true, nullptr);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        EXPECT_TRUE(is.pendingTupletHere());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDivide)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // triplet, half: three quarters in the time of a half
        setPendingTriplet(score, false);
        pressDuration(score, DurationType::V_HALF);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 r4 r4] r2");
        EXPECT_FALSE(is.pendingTuplet());
        EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);

        delete score;
    }
}

// like a tuplet command in By duration or Rhythm input, for any ratio
static void setPendingTuplet(Score* score, const Fraction& ratio, bool durationAsSubdivision)
{
    setPendingTriplet(score, durationAsSubdivision);
    InputState& is = score->inputState();
    PendingTuplet pending = *is.pendingTuplet();
    pending.ratio = ratio;
    is.setPendingTuplet(pending);
}

TEST_F(Engraving_TupletTests, pendingTupletOtherRatios)
{
    struct Case {
        Fraction ratio;
        bool durationAsSubdivision;
        DurationType duration;
        int dots;
        std::string expected;
        DurationType inputDuration;
    };
    const std::vector<Case> cases {
        // quintuplet, sixteenth: five sixteenths in the time of four
        { Fraction(5, 4), true, DurationType::V_16TH, 0, "5:4[16 r16 r16 r16 r16] r4 r2", DurationType::V_16TH },
        // quintuplet, quarter: a quarter divided into five
        { Fraction(5, 4), false, DurationType::V_QUARTER, 0, "5:4[16 r16 r16 r16 r16] r4 r2", DurationType::V_16TH },
        // duplet, eighth: two eighths in the time of three
        { Fraction(2, 3), true, DurationType::V_EIGHTH, 0, "2:3[8 r8] r8 r2", DurationType::V_EIGHTH },
        // duplet, dotted quarter: a dotted quarter divided into two
        { Fraction(2, 3), false, DurationType::V_QUARTER, 1, "2:3[8 r8] r8 r2", DurationType::V_EIGHTH },
    };
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (const Case& c : cases) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            is.setDots(c.dots);
            setPendingTuplet(score, c.ratio, c.durationAsSubdivision);
            EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, c.duration));
            pressDuration(score, c.duration);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), c.expected);
            EXPECT_FALSE(is.pendingTuplet());
            EXPECT_EQ(is.duration().type(), c.inputDuration);
            EXPECT_EQ(is.duration().dots(), 0);

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletRest)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        setPendingTriplet(score, true);
        is.setRest(true);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[r8 r8 r8] r4 r2");
        EXPECT_FALSE(is.pendingTuplet());
        EXPECT_EQ(is.tick(), Fraction(1, 12));

        // notes after the rest go into the tuplet
        is.setRest(false);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[r8 8 r8] r4 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletMixedDurations)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);

        // triplet, eighth, then a quarter fills the tuplet, and the next eighth comes after it
        setPendingTriplet(score, true);
        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_QUARTER);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4] r4 r2");
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4] 8 r8 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDoesNotFit)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a quarter on beat 4, then a quarter triplet would cross the barline
        pressDuration(score, DurationType::V_HALF);
        pressDuration(score, DurationType::V_QUARTER);
        setPendingTriplet(score, true);
        EXPECT_FALSE(NoteInput::canAddPendingTuplet(score, DurationType::V_QUARTER));
        pressDuration(score, DurationType::V_QUARTER);
        EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_TUPLET);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
        EXPECT_TRUE(is.pendingTupletHere());
        EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);

        // a rest neither
        is.setRest(true);
        pressDuration(score, DurationType::V_QUARTER);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
        EXPECT_TRUE(is.pendingTupletHere());
        is.setRest(false);

        // an eighth triplet fits
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 3:2[8 r8 r8]");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletKeepsDots)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a dotted input duration stays dotted, as without a tuplet: a dotted eighth triplet
        is.setDots(1);
        setPendingTriplet(score, true);
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8. r8. r8.] r8 r2");
        EXPECT_EQ(is.duration().dots(), 1);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletMovedAway)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // moving the input position cancels the tuplet: the duration enters a plain note
        setPendingTriplet(score, true);
        is.setSegment(score->tick2segment(Fraction(1, 1), false, SegmentType::ChordRest));
        EXPECT_FALSE(is.pendingTupletHere());
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_EQ(rhythm(measureAt(score, 1)), "8 r8 r4 r2");
        EXPECT_FALSE(is.pendingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletFollowsDottedPreviousNote)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // dotting the previous note moves the input position: the tuplet waits there instead
        pressDuration(score, DurationType::V_QUARTER);
        pressDuration(score, DurationType::V_QUARTER);
        setPendingTriplet(score, true);
        const EngravingItem* secondNote = toChord(chordRestAt(measureAt(score, 0), 1))->upNote();
        ASSERT_EQ(score->selection().element(), secondNote);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::toggleDots(tx, score, 1, true /*toggleForSelectionOnly*/);
        });
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4. r8 r4");
        EXPECT_EQ(is.tick(), Fraction(5, 8));
        EXPECT_TRUE(is.pendingTupletHere());

        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4. 3:2[8 r8 r8] r8");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletBeyondScore)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // past the last measure, the tuplet goes in a new measure
        const int measures = score->nmeasures();
        is.setSegment(score->lastMeasure()->first(SegmentType::ChordRest));
        is.setBeyondScore(true);
        setPendingTriplet(score, true);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        ASSERT_EQ(score->nmeasures(), measures + 1);
        EXPECT_EQ(rhythm(score->lastMeasure()), "3:2[8 r8 r8] r4 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNested)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a quarter triplet on beats 1 and 2
        setPendingTriplet(score, true);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_QUARTER);
        }
        ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] r2");

        // on its last quarter, another quarter triplet doesn't fit
        is.setSegment(score->tick2segment(Fraction(1, 3), false, SegmentType::ChordRest));
        setPendingTriplet(score, true);
        EXPECT_FALSE(NoteInput::canAddPendingTuplet(score, DurationType::V_QUARTER));

        // on its second quarter, an eighth triplet replaces it
        is.setSegment(score->tick2segment(Fraction(1, 6), false, SegmentType::ChordRest));
        setPendingTriplet(score, true);
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 3:2[8 r8 r8] 4] r2");
        EXPECT_EQ(is.tick(), Fraction(2, 9));

        delete score;
    }
}

// quarters in voice 2, as after a switch to voice 1: the selection is in voice 2, and voice 1 has an
// input position on every beat
static void enterVoice2Quarters(Score* score)
{
    InputState& is = score->inputState();
    is.setTrack(1);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    for (int i = 0; i < 4; ++i) {
        pressDuration(score, DurationType::V_QUARTER);
    }
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
}

TEST_F(Engraving_TupletTests, pendingTupletOtherVoiceSelected)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // the tuplet and its first note go in the input voice
        enterVoice2Quarters(score);
        setPendingTriplet(score, true);
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
        EXPECT_EQ(is.track(), 0);
        EXPECT_EQ(is.tick(), Fraction(1, 12));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsideLongerChordRest)
{
    // the tuplet waits on beat 2, inside a half note or rest: it shortens it, as a plain note does
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (bool rest : { false, true }) {
            for (DurationType duration : { DurationType::V_EIGHTH, DurationType::V_QUARTER }) {
                MasterScore* score = pendingTupletScore(method);
                ASSERT_TRUE(score);
                InputState& is = score->inputState();

                is.setRest(rest);
                pressDuration(score, DurationType::V_HALF);
                is.setRest(false);
                enterVoice2Quarters(score);
                is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
                ASSERT_FALSE(is.cr());

                setPendingTriplet(score, true);
                EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, duration));
                pressDuration(score, duration);
                EXPECT_TRUE(score->sanityCheck());
                const std::string first = rest ? "r4" : "4";
                if (duration == DurationType::V_EIGHTH) {
                    EXPECT_EQ(rhythm(measureAt(score, 0)), first + " 3:2[8 r8 r8] r2");
                    EXPECT_EQ(is.tick(), Fraction(1, 3));
                } else {
                    EXPECT_EQ(rhythm(measureAt(score, 0)), first + " 3:2[4 r4 r4] r4");
                    EXPECT_EQ(is.tick(), Fraction(5, 12));
                }
                EXPECT_FALSE(is.pendingTuplet());
                EXPECT_EQ(is.track(), 0);

                delete score;
            }
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsideLongerChordRestInTuplet)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a quarter triplet on beats 1 and 2
        setPendingTriplet(score, true);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_QUARTER);
        }
        enterVoice2Quarters(score);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] r2");

        // the tuplet waits on beat 2, inside the second quarter: it fits up to the end of the quarter triplet
        is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        ASSERT_FALSE(is.cr());
        setPendingTriplet(score, true);
        EXPECT_FALSE(NoteInput::canAddPendingTuplet(score, DurationType::V_QUARTER));
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 8 3:2[8 r8 r8] 8] r2");
        EXPECT_FALSE(is.pendingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDrums)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tupletDrums.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);

    // a snare (voice 1) takes the tuplet
    is.setNotes({ NoteVal(38) });
    setPendingTriplet(score, true);
    pressDuration(score, DurationType::V_EIGHTH);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");

    // a bass drum (voice 2) cancels it, as a voice change does: no empty tuplet in voice 1
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
    is.setNotes({ NoteVal(36) });
    setPendingTriplet(score, true);
    pressDuration(score, DurationType::V_EIGHTH);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
    const Segment* s = score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest);
    ASSERT_TRUE(s && s->element(1));
    EXPECT_TRUE(s->element(1)->isChord());
    EXPECT_FALSE(toChordRest(s->element(1))->tuplet());

    // where the tuplet doesn't fit, a snare is refused, but a bass drum isn't: it cancels the tuplet
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
    setPendingTriplet(score, true);
    is.setNotes({ NoteVal(38) });
    EXPECT_FALSE(NoteInput::canAddPendingTuplet(score, DurationType::V_HALF));
    is.setNotes({ NoteVal(36) });
    EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_HALF));

    delete score;
}

//-----------------------------------------
//    pending tuplets with dotted durations
//     single and double dots, before and after the
//     tuplet command
//-----------------------------------------

static void pressDot(Score* score, int dots)
{
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        NoteInput::toggleDots(tx, score, dots);
    });
}

TEST_F(Engraving_TupletTests, pendingTupletDoubleDots)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a double-dotted eighth triplet
        is.setDots(2);
        setPendingTriplet(score, true);
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8.. r8.. r8..] r16 r2");
        EXPECT_EQ(is.duration().dots(), 2);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDotAfterCommand)
{
    // the dot pressed while the tuplet waits: the same as before the tuplet command
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            setPendingTriplet(score, true);
            pressDot(score, dots);
            EXPECT_TRUE(is.pendingTupletHere());
            EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
            pressDuration(score, DurationType::V_EIGHTH);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[8. r8. r8.] r8 r2" : "3:2[8.. r8.. r8..] r16 r2");

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDotToggledOff)
{
    // a dot pressed again while the tuplet waits takes it away; the other dot replaces it
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            setPendingTriplet(score, true);
            pressDot(score, dots);
            pressDot(score, dots);
            EXPECT_EQ(is.duration().dots(), 0);
            pressDot(score, 3 - dots);
            pressDot(score, dots);
            EXPECT_TRUE(is.pendingTupletHere());
            EXPECT_EQ(is.duration().dots(), dots);
            pressDuration(score, DurationType::V_EIGHTH);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[8. r8. r8.] r8 r2" : "3:2[8.. r8.. r8..] r16 r2");

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDottedRest)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            is.setDots(dots);
            is.setRest(true);
            setPendingTriplet(score, true);
            pressDuration(score, DurationType::V_EIGHTH);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[r8. r8. r8.] r8 r2" : "3:2[r8.. r8.. r8..] r16 r2");
            EXPECT_FALSE(is.pendingTuplet());

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDottedDivide)
{
    // a dotted length is divided like a selected dotted note: plain notes where they fit
    // triplet, dotted half: three quarters (3:3); triplet, double-dotted half: three eighths in the time of 7
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            is.setDots(dots);
            setPendingTriplet(score, false);
            pressDuration(score, DurationType::V_HALF);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:3[4 r4 r4] r4" : "3:7[8 r8 r8] r8");
            EXPECT_FALSE(is.pendingTuplet());
            EXPECT_EQ(is.duration().type(), dots == 1 ? DurationType::V_QUARTER : DurationType::V_EIGHTH);
            EXPECT_EQ(is.duration().dots(), 0);

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDottedDoesNotFit)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a half note, then a dotted quarter triplet would cross the barline, though a plain one fits
        pressDuration(score, DurationType::V_HALF);
        setPendingTriplet(score, true);
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_QUARTER));
        is.setDots(1);
        EXPECT_FALSE(NoteInput::canAddPendingTuplet(score, DurationType::V_QUARTER));
        pressDuration(score, DurationType::V_QUARTER);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 r2");
        EXPECT_TRUE(is.pendingTupletHere());

        // a double-dotted eighth triplet fits
        is.setDots(2);
        EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_EIGHTH));
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 3:2[8.. r8.. r8..] r16");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletDottedUndo)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();
            const std::string dotted = dots == 1 ? "8." : "8..";
            const std::string after = dots == 1 ? " r8 r2" : " r16 r2";

            is.setDots(dots);
            setPendingTriplet(score, true);
            pressDuration(score, DurationType::V_EIGHTH);
            pressDuration(score, DurationType::V_EIGHTH);
            pressDuration(score, DurationType::V_EIGHTH);
            EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[" + dotted + " " + dotted + " " + dotted + "]" + after);

            // undoing the first note takes the tuplet away too, and the dotted tuplet waits again
            score->undoRedo(true, nullptr);
            score->undoRedo(true, nullptr);
            score->undoRedo(true, nullptr);
            EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
            EXPECT_TRUE(is.pendingTupletHere());
            EXPECT_EQ(is.duration().dots(), dots);
            pressDuration(score, DurationType::V_EIGHTH);
            EXPECT_TRUE(score->sanityCheck());
            EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[" + dotted + " r" + dotted + " r" + dotted + "]" + after);

            delete score;
        }
    }
}

//-----------------------------------------
//    tupletPreview
//     the tuplet that the next note or rest creates, shown while
//     note input waits
//-----------------------------------------

TEST_F(Engraving_TupletTests, tupletPreviewPending)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // nothing waits
        EXPECT_FALSE(NoteInput::tupletPreview(score));

        // a triplet waits for the next duration
        setPendingTriplet(score, true);
        std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
        ASSERT_TRUE(preview);
        EXPECT_EQ(preview->tick, Fraction(0, 1));
        ASSERT_EQ(preview->levels.size(), 1u);
        EXPECT_EQ(preview->levels[0].ratio, Fraction(3, 2));
        EXPECT_EQ(preview->levels[0].numberType, TupletNumberType::SHOW_NUMBER);

        // the next duration adds it
        pressDuration(score, DurationType::V_EIGHTH);
        EXPECT_FALSE(NoteInput::tupletPreview(score));

        // after the triplet, with the number type it was asked for
        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_EIGHTH);
        for (TupletNumberType numberType : { TupletNumberType::SHOW_RELATION, TupletNumberType::NO_TEXT }) {
            setPendingTriplet(score, true);
            PendingTuplet pending = *is.pendingTuplet();
            pending.numberType = numberType;
            is.setPendingTuplet(pending);
            preview = NoteInput::tupletPreview(score);
            ASSERT_TRUE(preview);
            EXPECT_EQ(preview->tick, Fraction(1, 4));
            EXPECT_EQ(preview->levels[0].numberType, numberType);
        }

        // moving away cancels it
        is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
        EXPECT_FALSE(NoteInput::tupletPreview(score));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingBeyondScore)
{
    MasterScore* score = pendingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // past the last measure, where the next duration appends a measure
    is.setSegment(score->lastMeasure()->first(SegmentType::ChordRest));
    is.setBeyondScore(true);
    setPendingTriplet(score, true);
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_EQ(preview->tick, score->endTick());

    delete score;
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingRatioDotted)
{
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a dotted or double-dotted value as the subdivision: always 3:2
        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        is.setDuration(eighth);
        setPendingTriplet(score, true);
        std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
        ASSERT_TRUE(preview);
        EXPECT_EQ(preview->levels[0].ratio, Fraction(3, 2));

        // a dotted or double-dotted half divided: the ratio follows it
        is.setPendingTuplet(std::nullopt);
        TDuration half(DurationType::V_HALF);
        half.setDots(dots);
        is.setDuration(half);
        setPendingTriplet(score, false);
        preview = NoteInput::tupletPreview(score);
        ASSERT_TRUE(preview);
        EXPECT_EQ(preview->levels[0].ratio, dots == 1 ? Fraction(3, 3) : Fraction(3, 7));

        delete score;
    }
}

//-----------------------------------------
//    pending tuplets in By note name input
//     the tuplet waits, sized by the input duration, and
//     the next note or rest adds it, in an empty 4/4 score
//-----------------------------------------

static void enterNote(Score* score, int pitch = 60, bool addToChord = false)
{
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        NoteVal nval(pitch);
        NoteInput::addPitch(tx, score, nval, addToChord);
    });
}

static void enterRest(Score* score)
{
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction&) {
        score->enterRest(score->inputState().duration());
    });
}

// like R (repeat) in note input: the chord again at the input position, as a new chord
static void repeatChord(Score* score, const Chord* chord)
{
    std::vector<NoteVal> notes;
    for (const Note* note : chord->notes()) {
        notes.push_back(note->noteVal());
    }
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        for (size_t i = 0; i < notes.size(); ++i) {
            NoteVal nval = notes[i];
            if (!NoteInput::addPitch(tx, score, nval, i > 0) && i == 0) {
                break;
            }
        }
    });
}

// like a tuplet command in By note name input
static bool waitForTuplet(Score* score, bool durationAsSubdivision, const Fraction& ratio = Fraction(3, 2))
{
    PendingTuplet pending;
    pending.ratio = ratio;
    pending.numberType = TupletNumberType::SHOW_NUMBER;
    pending.bracketType = TupletBracketType::AUTO_BRACKET;
    pending.autoBaseLen = true;
    pending.durationAsSubdivision = durationAsSubdivision;
    return NoteInput::setPendingTuplet(score, pending);
}

TEST_F(Engraving_TupletTests, pendingTupletNoteName)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // eighth, triplet: nothing is added until the next note
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(is.tick(), Fraction(1, 12));
    enterNote(score);
    enterNote(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] r4 r2");

    // undoing the first note takes the tuplet away too, and the tuplet waits again
    for (int i = 0; i < 3; ++i) {
        score->undoRedo(true, nullptr);
    }
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_TRUE(is.pendingTupletHere());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameUndoAfterEscape)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // after Escape, undoing the first note doesn't bring the tuplet back
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.forgetPendingTuplets();
    for (int i = 0; i < 3; ++i) {
        score->undoRedo(true, nullptr);
    }
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_FALSE(is.pendingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDivide)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // quarter, triplet: the quarter divided into three eighths, the input duration
    ASSERT_TRUE(waitForTuplet(score, false));
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameFirstNote)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // eighth, triplet, then a quarter is the first note of the eighth triplet, not a quarter triplet
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(NoteInput::canAddPendingTuplet(score, DurationType::V_WHOLE));
    pressDuration(score, DurationType::V_QUARTER);
    EXPECT_TRUE(is.pendingTupletHere());
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 r8] r4 r2");

    pressDuration(score, DurationType::V_EIGHTH);
    enterNote(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 8] r4 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameAgain)
{
    // a tuplet asked for again while one waits: the same duration keeps what sized the waiting one
    struct Case {
        DurationType first;
        bool durationAsSubdivision;
        DurationType changeTo;
        Fraction ratio;
        std::string expected;
    };
    const std::vector<Case> cases {
        // eighth, triplet, quintuplet: an eighth quintuplet
        { DurationType::V_EIGHTH, true, DurationType::V_INVALID, Fraction(5, 4), "5:4[8 r8 r8 r8 r8] r2" },
        // eighth, triplet, quarter, triplet: a quarter triplet
        { DurationType::V_EIGHTH, true, DurationType::V_QUARTER, Fraction(3, 2), "3:2[4 r4 r4] r2" },
        // quarter divided into a triplet, then a quintuplet: the quarter divided into five
        { DurationType::V_QUARTER, false, DurationType::V_INVALID, Fraction(5, 4), "5:4[16 r16 r16 r16 r16] r4 r2" },
        // quarter divided into a triplet, then half, triplet: the half divided into three
        { DurationType::V_QUARTER, false, DurationType::V_HALF, Fraction(3, 2), "3:2[4 r4 r4] r2" },
    };
    for (const Case& c : cases) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);

        score->inputState().setDuration(c.first);
        ASSERT_TRUE(waitForTuplet(score, c.durationAsSubdivision));
        if (c.changeTo != DurationType::V_INVALID) {
            pressDuration(score, c.changeTo);
        }
        ASSERT_TRUE(waitForTuplet(score, c.durationAsSubdivision, c.ratio));
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), c.expected);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDoesNotFit)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a half and a quarter, then a quarter triplet on beat 4 doesn't fit: it waits all the same
    is.setDuration(DurationType::V_HALF);
    enterNote(score);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));

    // a note is refused, and the tuplet keeps waiting
    enterNote(score);
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
    EXPECT_TRUE(is.pendingTupletHere());

    // a duration key doesn't change it
    pressDuration(score, DurationType::V_EIGHTH);
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
    enterNote(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");

    // the tuplet command again, with the eighth: an eighth triplet, which fits, replaces it
    MScore::setError(MsError::MS_NO_ERROR);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 3:2[8 r8 r8]");

    // undoing that note brings back the eighth triplet, not the one that didn't fit
    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDoesNotFitEntryPaths)
{
    // where the tuplet doesn't fit, each way of entering a note or rest is refused, and it keeps waiting
    const std::vector<std::function<void(Score*)> > paths {
        // letter
        [](Score* score) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
            });
        },
        // MIDI
        [](Score* score) { enterNote(score); },
        // mouse, as for a tablature fret
        [](Score* score) {
            Position pos;
            pos.segment = score->tick2segment(Fraction(3, 4), false, SegmentType::ChordRest);
            pos.staffIdx = 0;
            pos.line = 4;
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::putNote(tx, score, pos, false);
            });
        },
        // rest
        [](Score* score) { enterRest(score); },
        // R
        [](Score* score) { repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 1))); },
        // tie
        [](Score* score) { EditTie::cmdAddTie(score); },
    };
    for (size_t i = 0; i < paths.size(); ++i) {
        SCOPED_TRACE(i);
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_HALF);
        enterNote(score);
        is.setDuration(DurationType::V_QUARTER);
        enterNote(score);
        ASSERT_TRUE(waitForTuplet(score, true));
        paths[i](score);
        EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
        MScore::setError(MsError::MS_NO_ERROR);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
        EXPECT_TRUE(is.pendingTupletHere());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDoesNotFitDrums)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tupletDrums.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);

    auto enterDrum = [score](int drumPitch) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::addPitch(tx, score, NoteInputParams { 0, drumPitch }, false, false);
        });
    };

    // two snares, then a quarter triplet on beat 4: a snare (voice 1) is refused, and the tuplet keeps waiting
    is.setDuration(DurationType::V_HALF);
    enterDrum(38);
    is.setDuration(DurationType::V_QUARTER);
    enterDrum(38);
    ASSERT_TRUE(waitForTuplet(score, true));
    enterDrum(38);
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
    MScore::setError(MsError::MS_NO_ERROR);
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");

    // a bass drum (voice 2) cancels it, as a voice change does, and goes in
    enterDrum(36);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_FALSE(is.pendingTuplet());
    const Segment* s = score->tick2segment(Fraction(3, 4), false, SegmentType::ChordRest);
    ASSERT_TRUE(s && s->element(1));
    EXPECT_TRUE(toChordRest(s->element(1))->isChord());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameRest)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    enterRest(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[r8 r8 r8] r4 r2");
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(is.tick(), Fraction(1, 12));

    enterNote(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[r8 8 r8] r4 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameEntryPaths)
{
    // a note entered by letter, by MIDI or with the mouse
    const std::vector<std::function<void(Score*)> > paths {
        [](Score* score) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
            });
        },
        [](Score* score) { enterNote(score); },
        [](Score* score) {
            Position pos;
            pos.segment = score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest);
            pos.staffIdx = 0;
            pos.line = 4;
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::putNote(tx, score, pos, false);
            });
        },
    };
    for (const auto& enter : paths) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        enter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
        EXPECT_FALSE(is.pendingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameMovedAway)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // the input position moves to bar 2: an ordinary note there
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    is.setSegment(score->tick2segment(Fraction(1, 1), false, SegmentType::ChordRest));
    enterNote(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_EQ(rhythm(measureAt(score, 1)), "8 r8 r4 r2");
    EXPECT_FALSE(is.pendingTuplet());

    // a note entered with the mouse elsewhere: the same
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    ASSERT_TRUE(waitForTuplet(score, true));
    Position pos;
    pos.segment = score->tick2segment(Fraction(2, 1), false, SegmentType::ChordRest);
    pos.staffIdx = 0;
    pos.line = 4;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        NoteInput::putNote(tx, score, pos, false);
    });
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_EQ(rhythm(measureAt(score, 2)), "8 r8 r4 r2");
    EXPECT_FALSE(is.pendingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameBeyondScore)
{
    // past the last measure, by MIDI or by letter: the tuplet goes in a new measure
    for (bool byLetter : { false, true }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        const int measures = score->nmeasures();
        is.setSegment(score->lastMeasure()->first(SegmentType::ChordRest));
        is.setBeyondScore(true);
        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        if (byLetter) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
            });
        } else {
            enterNote(score);
        }
        EXPECT_TRUE(score->sanityCheck());
        ASSERT_EQ(score->nmeasures(), measures + 1);
        EXPECT_EQ(rhythm(score->lastMeasure()), "3:2[8 r8 r8] r4 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameOtherVoiceSelected)
{
    MasterScore* score = pendingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // the tuplet and its first note go in the input voice
    enterVoice2Quarters(score);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
    EXPECT_EQ(is.track(), 0);
    EXPECT_EQ(is.tick(), Fraction(1, 12));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameInsideLongerChordRest)
{
    // the tuplet waits on beat 2, inside a half note or rest: it shortens it, as a plain note does
    for (bool rest : { false, true }) {
        MasterScore* score = pendingTupletScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setRest(rest);
        pressDuration(score, DurationType::V_HALF);
        is.setRest(false);
        enterVoice2Quarters(score);
        is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        ASSERT_FALSE(is.cr());

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), std::string(rest ? "r4" : "4") + " 3:2[8 r8 r8] r2");
        EXPECT_EQ(is.track(), 0);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletRepeat)
{
    // By note name: R after a chord takes the waiting tuplet, as any next note does
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setDuration(DurationType::V_EIGHTH);
    enterNote(score, 60);
    enterNote(score, 64, true);
    ASSERT_TRUE(waitForTuplet(score, true));
    repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 0)));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 3:2[8 r8 r8] r8 r2");
    EXPECT_FALSE(is.pendingTuplet());
    const ChordRest* repeated = chordRestAt(measureAt(score, 0), 1);
    ASSERT_TRUE(repeated && repeated->isChord());
    EXPECT_EQ(toChord(repeated)->notes().size(), 2u);
    EXPECT_TRUE(repeated->tuplet());
    delete score;

    // By duration: R isn't a duration, so it enters an ordinary chord, and the tuplet no longer waits there
    score = pendingTupletScore(NoteEntryMethod::BY_DURATION);
    ASSERT_TRUE(score);
    InputState& is2 = score->inputState();
    is2.setNotes({ NoteVal(60), NoteVal(64) });
    pressDuration(score, DurationType::V_EIGHTH);
    ASSERT_EQ(rhythm(measureAt(score, 0)), "8 r8 r4 r2");
    setPendingTriplet(score, true);
    repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 0)));
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 r4 r2");
    EXPECT_FALSE(is2.pendingTupletHere());
    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameAddToChord)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a note added to the chord just entered leaves the tuplet waiting after it
    is.setDuration(DurationType::V_EIGHTH);
    enterNote(score, 60);
    ASSERT_TRUE(waitForTuplet(score, true));
    enterNote(score, 64, true);
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 r8 r4 r2");
    EXPECT_EQ(toChord(chordRestAt(measureAt(score, 0), 0))->notes().size(), 2u);

    enterNote(score, 67);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 3:2[8 r8 r8] r8 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameTie)
{
    // after a chord: the tie key ties both notes into the waiting triplet
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setDuration(DurationType::V_EIGHTH);
    enterNote(score, 60);
    enterNote(score, 64, true);
    ASSERT_TRUE(waitForTuplet(score, true));
    EditTie::cmdAddTie(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8~ 3:2[8 r8 r8] r8 r2");
    EXPECT_FALSE(is.pendingTuplet());
    const ChordRest* tied = chordRestAt(measureAt(score, 0), 1);
    ASSERT_TRUE(tied && tied->isChord());
    EXPECT_EQ(toChord(tied)->notes().size(), 2u);
    for (const Note* note : toChord(tied)->notes()) {
        EXPECT_TRUE(note->tieBack());
    }
    delete score;

    // with the input position moved back onto a note (D, after the chord), the triplet waits there and takes a note
    // tied from the chord before it, rather than the selected D being tied into the next position
    score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is2 = score->inputState();
    is2.setDuration(DurationType::V_EIGHTH);
    enterNote(score, 60);
    enterNote(score, 64, true);
    enterNote(score, 62);
    enterNote(score, 65);
    is2.setSegment(score->tick2segment(Fraction(1, 8), false, SegmentType::ChordRest));
    score->select(toChord(is2.cr())->upNote());
    ASSERT_TRUE(waitForTuplet(score, true));
    EditTie::cmdAddTie(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8~ 3:2[8 r8 r8] r8 r2");
    EXPECT_FALSE(is2.pendingTuplet());
    const ChordRest* chord = chordRestAt(measureAt(score, 0), 0);
    tied = chordRestAt(measureAt(score, 0), 1);
    ASSERT_TRUE(chord && chord->isChord() && tied && tied->isChord());
    EXPECT_TRUE(tied->tuplet());
    ASSERT_EQ(toChord(tied)->notes().size(), 2u);
    ASSERT_TRUE(toChord(tied)->downNote()->tieBack() && toChord(tied)->upNote()->tieBack());
    EXPECT_EQ(toChord(tied)->downNote()->tieBack()->startNote(), toChord(chord)->downNote());
    EXPECT_EQ(toChord(tied)->upNote()->tieBack()->startNote(), toChord(chord)->upNote());
    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDrums)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tupletDrums.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);

    auto enterDrum = [score](int drumPitch) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::addPitch(tx, score, NoteInputParams { 0, drumPitch }, false, false);
        });
    };

    // a snare (voice 1) takes the tuplet
    ASSERT_TRUE(waitForTuplet(score, true));
    enterDrum(38);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");

    // a bass drum (voice 2) cancels it, as a voice change does: no empty tuplet in voice 1
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
    ASSERT_TRUE(waitForTuplet(score, true));
    enterDrum(36);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2");
    const Segment* s = score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest);
    ASSERT_TRUE(s && s->element(1));
    const ChordRest* bassDrum = toChordRest(s->element(1));
    EXPECT_TRUE(bassDrum->isChord());
    EXPECT_FALSE(bassDrum->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDotPreviousNote)
{
    // a half and two eighths, then an eighth triplet waits on beat 4; dotting or double-dotting the second eighth
    // moves the input position to 13/16 or 27/32, where the triplet doesn't fit: it keeps waiting there
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_HALF);
        enterNote(score);
        is.setDuration(DurationType::V_EIGHTH);
        enterNote(score);
        enterNote(score);
        ASSERT_TRUE(waitForTuplet(score, true));

        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::toggleDots(tx, score, dots, true /*toggleForSelectionOnly*/);
        });
        if (dots == 1) {
            EXPECT_EQ(rhythm(measureAt(score, 0)), "2 8 8. r16 r8");
        }
        EXPECT_TRUE(is.pendingTupletHere());
        EXPECT_EQ(is.pendingTuplet()->tick, dots == 1 ? Fraction(13, 16) : Fraction(27, 32));
        EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));

        delete score;
    }
}

//-----------------------------------------
//    pending tuplets in By note name input with dotted
//     durations: single and double dots, before and after
//     the tuplet command
//-----------------------------------------

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDottedNoteValue)
{
    // the dots chosen before the tuplet command: a dotted or double-dotted eighth triplet
    for (int dots : { 1, 2 }) {
        for (bool rest : { false, true }) {
            MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            TDuration eighth(DurationType::V_EIGHTH);
            eighth.setDots(dots);
            is.setDuration(eighth);
            ASSERT_TRUE(waitForTuplet(score, true));
            if (rest) {
                enterRest(score);
            } else {
                enterNote(score);
            }
            EXPECT_TRUE(score->sanityCheck());
            const std::string r = rest ? "r" : "";
            EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[" + r + "8. r8. r8.] r8 r2" : "3:2[" + r + "8.. r8.. r8..] r16 r2");
            EXPECT_EQ(is.duration().dots(), dots);

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDotAfterCommand)
{
    // eighth, triplet, then a dot: the first note is dotted, in the eighth triplet
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        pressDot(score, dots);
        EXPECT_TRUE(is.pendingTupletHere());
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[8. r16 r8] r4 r2" : "3:2[8.. r32 r8] r4 r2");
        EXPECT_EQ(is.tick(), dots == 1 ? Fraction(1, 8) : Fraction(7, 48));
        const ChordRest* first = chordRestAt(measureAt(score, 0), 0);
        ASSERT_TRUE(first && first->tuplet());
        EXPECT_EQ(first->tuplet()->baseLen().type(), DurationType::V_EIGHTH);
        EXPECT_EQ(first->tuplet()->baseLen().dots(), 0);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDottedDivide)
{
    // triplet, dotted half: three quarters (3:3); triplet, double-dotted half: three eighths in the time of 7
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        TDuration half(DurationType::V_HALF);
        half.setDots(dots);
        is.setDuration(half);
        ASSERT_TRUE(waitForTuplet(score, false));
        EXPECT_EQ(is.duration().type(), dots == 1 ? DurationType::V_QUARTER : DurationType::V_EIGHTH);
        EXPECT_EQ(is.duration().dots(), 0);
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:3[4 r4 r4] r4" : "3:7[8 r8 r8] r8");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDottedAgain)
{
    // dotted or double-dotted eighth, triplet, quintuplet: a dotted or double-dotted eighth quintuplet
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);

        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        score->inputState().setDuration(eighth);
        ASSERT_TRUE(waitForTuplet(score, true));
        ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "5:4[8. r8. r8. r8. r8.] r4" : "5:4[8.. r8.. r8.. r8.. r8..] r8");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDottedDoesNotFit)
{
    // on beat 3, a dotted or double-dotted quarter triplet doesn't fit: it waits, notes are refused, and the
    // dotted or double-dotted eighth and the tuplet command again replace it with one that fits
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_HALF);
        enterNote(score);
        TDuration quarter(DurationType::V_QUARTER);
        quarter.setDots(dots);
        is.setDuration(quarter);
        ASSERT_TRUE(waitForTuplet(score, true));
        EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
        enterNote(score);
        EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
        MScore::setError(MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "2 r2");

        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        is.setDuration(eighth);
        ASSERT_TRUE(waitForTuplet(score, true));
        EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "2 3:2[8. r8. r8.] r8" : "2 3:2[8.. r8.. r8..] r16");

        delete score;
    }
}

//-----------------------------------------
//    pending tuplets in Insert input
//     the tuplet waits as in By note name input, and the
//     next note or rest inserts its whole time: the bar
//     grows and the following notes move right. Four
//     quarters in 4/4, input position on `tick`.
//-----------------------------------------

static MasterScore* pendingTupletInsertScore(const Fraction& tick = Fraction(1, 4))
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    if (!score) {
        return nullptr;
    }
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    InputState& is = score->inputState();
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(tick, false, SegmentType::ChordRest));
    // Insert needs a selected note or rest
    const ChordRest* cr = is.cr();
    score->select(cr->isChord() ? static_cast<EngravingItem*>(toChord(cr)->upNote()) : const_cast<ChordRest*>(cr));
    return score;
}

// a letter in Insert input, or with `insertFlag` Ctrl+Shift+letter (insert) in By note name input
static void insertLetter(Score* score, bool insertFlag = false)
{
    score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
        NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, insertFlag);
    });
}

TEST_F(Engraving_TupletTests, pendingTupletInsert)
{
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // eighth, triplet on beat 2: nothing changes until the next note
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 4");
    EXPECT_TRUE(is.pendingTupletHere());

    // the next note inserts the triplet's quarter, and goes into it
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(is.tick(), Fraction(1, 4) + Fraction(1, 12));

    // the next notes fill the triplet, as in By note name input; after it, Insert inserts again
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
    insertLetter(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 8] 8 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(11, 8));

    // undoing the first note takes the inserted time away too, and the tuplet waits again
    for (int i = 0; i < 4; ++i) {
        score->undoRedo(true, nullptr);
    }
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(1, 1));
    EXPECT_TRUE(is.pendingTupletHere());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertTie)
{
    // the input position, and the selection, on D
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // eighth, triplet, then the tie key: the triplet's time is inserted before D, and C is tied into it
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EditTie::cmdAddTie(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4~ 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
    EXPECT_FALSE(is.pendingTuplet());

    Measure* measure = measureAt(score, 0);
    const ChordRest* c = chordRestAt(measure, 0);
    const ChordRest* tied = chordRestAt(measure, 1);
    ASSERT_TRUE(c && c->isChord() && tied && tied->isChord());
    const Note* tiedNote = toChord(tied)->upNote();
    EXPECT_EQ(tiedNote->pitch(), 60);
    ASSERT_TRUE(tiedNote->tieBack());
    EXPECT_EQ(tiedNote->tieBack()->startNote(), toChord(c)->upNote());

    // D, E, F are unchanged
    for (int i = 0; i < 3; ++i) {
        const ChordRest* cr = chordRestAt(measure, 4 + i);
        ASSERT_TRUE(cr && cr->isChord());
        EXPECT_EQ(toChord(cr)->upNote()->pitch(), (std::vector<int> { 62, 64, 65 })[i]);
        EXPECT_FALSE(toChord(cr)->upNote()->tieBack());
    }
    delete score;

    // on the next bar's measure rest: tied from F, across the barline
    score = pendingTupletInsertScore(Fraction(1, 1));
    ASSERT_TRUE(score);
    score->inputState().setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EditTie::cmdAddTie(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 4~");
    EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[8 r8 r8] r4 r2 r4");
    EXPECT_EQ(measureAt(score, 1)->ticks(), Fraction(5, 4));
    const ChordRest* first = chordRestAt(measureAt(score, 1), 0);
    ASSERT_TRUE(first && first->isChord());
    ASSERT_TRUE(toChord(first)->upNote()->tieBack());
    EXPECT_EQ(toChord(first)->upNote()->tieBack()->startNote(), toChord(chordRestAt(measureAt(score, 0), 3))->upNote());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertEntryPaths)
{
    // a note entered by letter, by MIDI, with the mouse or on a TAB staff (putNote), or a rest: each inserts the tuplet's time
    const std::vector<std::pair<std::function<void(Score*)>, std::string> > paths {
        { [](Score* score) { insertLetter(score); }, "4 3:2[8 r8 r8] 4 4 4" },
        { [](Score* score) { enterNote(score); }, "4 3:2[8 r8 r8] 4 4 4" },
        { [](Score* score) {
                Position pos;
                pos.segment = score->inputState().segment();
                pos.staffIdx = 0;
                pos.line = 4;
                score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                    NoteInput::insertChordByInsertingTime(tx, score, pos);
                });
            }, "4 3:2[8 r8 r8] 4 4 4" },
        { [](Score* score) {
                Position pos;
                pos.segment = score->inputState().segment();
                pos.staffIdx = 0;
                pos.line = 4;
                score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                    NoteInput::putNote(tx, score, pos, true);
                });
            }, "4 3:2[8 r8 r8] 4 4 4" },
        { [](Score* score) { enterRest(score); }, "4 3:2[r8 r8 r8] 4 4 4" },
    };
    for (const auto& [enter, expected] : paths) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        enter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), expected);
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        EXPECT_FALSE(is.pendingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertAtBarEnd)
{
    // a quarter triplet on beat 4 wouldn't fit in By note name input; in Insert, the bar grows by a half
    MasterScore* score = pendingTupletInsertScore(Fraction(3, 4));
    ASSERT_TRUE(score);

    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 3:2[4 r4 r4] 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(6, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDivide)
{
    // quarter, triplet: the quarter divided into three eighths is inserted
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);

    ASSERT_TRUE(waitForTuplet(score, false));
    EXPECT_EQ(score->inputState().duration().type(), DurationType::V_EIGHTH);
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertAgain)
{
    // eighth, triplet, quintuplet: an eighth quintuplet's half is inserted
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);

    score->inputState().setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 5:4[8 r8 r8 r8 r8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(6, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertInsideTuplet)
{
    // a quarter triplet on beats 1-2, then Insert on its second note: no tuplet can wait there
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] 4 4");
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    const ChordRest* second = chordRestAt(measureAt(score, 0), 1);
    is.setSegment(second->segment());
    score->select(toChord(second)->upNote());
    is.setDuration(DurationType::V_EIGHTH);
    EXPECT_FALSE(waitForTuplet(score, true));
    EXPECT_FALSE(is.pendingTuplet());

    // and notes inside a tuplet that was already there are still refused
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::CANNOT_INSERT_TUPLET);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] 4 4");
    MScore::setError(MsError::MS_NO_ERROR);

    // at the triplet's start it can: the new tuplet goes before it
    const ChordRest* first = chordRestAt(measureAt(score, 0), 0);
    is.setSegment(first->segment());
    score->select(toChord(first)->upNote());
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] 3:2[4 4 4] 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertFlag)
{
    // in By note name input, inserting a note (Ctrl+Shift+letter) inserts the waiting tuplet's time too
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score, true);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertUndoAfterEscape)
{
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // after Escape, undoing the first note doesn't bring the tuplet back
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    is.forgetPendingTuplets();
    score->undoRedo(true, nullptr);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 4");
    EXPECT_FALSE(is.pendingTuplet());

    delete score;
}

// D, which followed the inserted time: still there, and not tied to a note before it
static void expectD(const ChordRest* cr)
{
    ASSERT_TRUE(cr && cr->isChord());
    EXPECT_EQ(toChord(cr)->upNote()->pitch(), 62);
    EXPECT_FALSE(toChord(cr)->upNote()->tieBack());
}

TEST_F(Engraving_TupletTests, pendingTupletInsertLongerNote)
{
    // eighth, triplet, half: the note ends with the triplet, and the notes after it stay;
    // in Insert input, and with Ctrl+Shift+letter in By note name input
    for (bool insertFlag : { false, true }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        if (insertFlag) {
            is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        }

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        pressDuration(score, DurationType::V_HALF);
        ASSERT_TRUE(is.pendingTupletHere());
        insertLetter(score, insertFlag);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4.] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        EXPECT_EQ(is.tick(), Fraction(1, 2));
        expectD(chordRestAt(measureAt(score, 0), 2));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertLongerNoteMidi)
{
    // the same with a note from a MIDI keyboard, or R (repeating C)
    for (bool repeat : { false, true }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        pressDuration(score, DurationType::V_HALF);
        ASSERT_TRUE(is.pendingTupletHere());
        if (repeat) {
            repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 0)));
        } else {
            enterNote(score, 67);
        }
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4.] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        EXPECT_EQ(is.tick(), Fraction(1, 2));
        expectD(chordRestAt(measureAt(score, 0), 2));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertLongerFillingNote)
{
    // a later note that fills the inserted triplet ends with it too
    for (bool insertFlag : { false, true }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        if (insertFlag) {
            is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        }

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        insertLetter(score, insertFlag);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
        pressDuration(score, DurationType::V_HALF);
        insertLetter(score, insertFlag);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 4] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        expectD(chordRestAt(measureAt(score, 0), 3));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertFillsOnlyRests)
{
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    insertLetter(score);
    ASSERT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 r8] 4 4 4");

    // back onto one of its notes: refused, as in any other tuplet
    const ChordRest* second = chordRestAt(measureAt(score, 0), 2);
    is.setSegment(second->segment());
    score->select(toChord(second)->upNote());
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::CANNOT_INSERT_TUPLET);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 r8] 4 4 4");
    MScore::setError(MsError::MS_NO_ERROR);

    // its rest is still filled
    const ChordRest* third = chordRestAt(measureAt(score, 0), 3);
    is.setSegment(third->segment());
    score->select(const_cast<ChordRest*>(third));
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 8] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertLeftAndBack)
{
    // after leaving note input and coming back, or changing the method and back, the inserted triplet is like any other
    for (bool leave : { true, false }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        insertLetter(score);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
        if (leave) {
            is.setNoteEntryMode(false);
            is.setNoteEntryMode(true);
        } else {
            is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
            is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
        }

        const ChordRest* rest = chordRestAt(measureAt(score, 0), 2);
        is.setSegment(rest->segment());
        score->select(const_cast<ChordRest*>(rest));
        MScore::setError(MsError::MS_NO_ERROR);
        insertLetter(score);
        EXPECT_EQ(MScore::_error, MsError::CANNOT_INSERT_TUPLET);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
        MScore::setError(MsError::MS_NO_ERROR);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertBeyondScore)
{
    // with the input position beyond the score: one new measure, which the tuplet starts
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setSegment(score->lastMeasure()->first(SegmentType::ChordRest));
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    ASSERT_TRUE(is.beyondScore());
    const int measures = score->nmeasures();
    const Fraction endTick = score->endTick();
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    score->select(toChord(chordRestAt(score->lastMeasure(), 3))->upNote());

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(score->nmeasures(), measures + 1);
    EXPECT_EQ(rhythm(score->lastMeasure()), "3:2[8 r8 r8] r4 r2");
    EXPECT_EQ(is.tick(), endTick + Fraction(1, 12));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertOtherVoiceTuplet)
{
    // a quarter triplet on beats 1-2 in voice 2: time inserted on beat 2 would split it
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    is.setTrack(1);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 55);
    }
    ASSERT_TRUE(score->sanityCheck());

    is.setTrack(0);
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setDuration(DurationType::V_EIGHTH);
    EXPECT_FALSE(waitForTuplet(score, true));
    EXPECT_FALSE(is.pendingTuplet());

    // on beat 1, where the triplet starts, the new tuplet goes before it
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] 4 4 4 4");
    EXPECT_EQ(rhythm(measureAt(score, 0), 1), "r4 3:2[4 4 4] r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertWhereOtherVoiceTupletStarts)
{
    // a quarter triplet on beats 2-3 in voice 2; a triplet inserted on beat 2 moves it to start at the input position,
    // where the next waiting tuplet is inserted before it too
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    is.setTrack(1);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 55);
    }
    ASSERT_TRUE(score->sanityCheck());

    is.setTrack(0);
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_EIGHTH);
    for (int round = 0; round < 2; ++round) {
        ASSERT_TRUE(waitForTuplet(score, true));
        for (int i = 0; i < 3; ++i) {
            insertLetter(score);
        }
    }
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 8] 3:2[8 8 8] 4 4 4");
    EXPECT_EQ(rhythm(measureAt(score, 0), 1), "r4 r4 r4 3:2[4 4 4] r4");
    const Segment* segment = score->tick2segment(Fraction(3, 4), false, SegmentType::ChordRest);
    const ChordRest* first = segment ? toChordRest(segment->element(1)) : nullptr;
    ASSERT_TRUE(first && first->tuplet());
    EXPECT_EQ(first->tuplet()->tick(), Fraction(3, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertRedo)
{
    // undo and redo of the first note: the next note fills the triplet
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    score->undoRedo(true, nullptr);
    score->undoRedo(false, nullptr);
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 r8] 4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertInPart)
{
    // entered in the score or in a part: the tuplet's time is inserted in both
    for (bool inPart : { false, true }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        score->startCmd(TranslatableString::untranslatable("Pending tuplet tests"));
        Score* part = score->createScore();
        Excerpt* excerpt = new Excerpt(score);
        excerpt->setExcerptScore(part);
        excerpt->setName(u"Piano");
        excerpt->setParts({ score->parts().at(0) });
        Excerpt::createExcerpt(excerpt);
        score->undo(new AddExcerpt(excerpt));
        score->endCmd();

        Score* s = inPart ? part : static_cast<Score*>(score);
        InputState& is = s->inputState();
        is.setTrack(0);
        is.setSegment(s->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
        is.setDuration(DurationType::V_QUARTER);
        is.setNotes({ NoteVal(60) });
        is.setNoteEntryMode(true);
        is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        for (int pitch : { 60, 62, 64, 65 }) {
            enterNote(s, pitch);
        }
        is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
        is.setSegment(s->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        s->select(toChord(is.cr())->upNote());

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(s, true));
        for (int i = 0; i < 3; ++i) {
            insertLetter(s);
        }
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(score->firstMeasure()), "4 3:2[8 8 8] 4 4 4");
        EXPECT_EQ(rhythm(part->firstMeasure()), "4 3:2[8 8 8] 4 4 4");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertLinkedTab)
{
    // a guitar staff with a linked TAB staff: the tuplet is on both
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"guitarLinkedTab.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    for (int pitch : { 64, 65, 67, 69 }) {
        enterNote(score, pitch);
    }
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 3; ++i) {
        insertLetter(score);
    }
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(score->firstMeasure()), "4 3:2[8 8 8] 4 4 4");
    EXPECT_EQ(rhythm(score->firstMeasure(), VOICES), "4 3:2[8 8 8] 4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertEmptyBar)
{
    // in a bar with a measure rest, the bar grows by the tuplet's time
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    score->select(const_cast<ChordRest*>(is.cr()));

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 r8 r8] r4 r2 r4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertEmptyVoice)
{
    // in an empty voice 2: voice 1 gets a rest for the inserted time
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    // after selecting, which moves the input position to the selected voice
    is.setTrack(1);

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 r4 4 4 4");
    EXPECT_EQ(rhythm(measureAt(score, 0), 1), "r4 3:2[8 r8 r8] r2 r4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDrums)
{
    // a drum in the same voice takes the tuplet; one in another voice cancels it, and is inserted as without one
    for (int drumPitch : { 38, 36 }) {
        MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tupletDrums.mscx");
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        is.setTrack(0);
        is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
        is.setDuration(DurationType::V_QUARTER);
        is.setNoteEntryMode(true);
        is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        auto enterDrum = [score](int pitch) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                NoteInput::addPitch(tx, score, NoteInputParams { 0, pitch }, false, false);
            });
        };
        for (int i = 0; i < 4; ++i) {
            enterDrum(38);
        }
        is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
        is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        score->select(toChord(is.cr())->upNote());

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        enterDrum(drumPitch);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_FALSE(is.pendingTuplet());
        if (drumPitch == 38) {
            EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 r8 r8] 4 4 4");
            EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        } else {
            EXPECT_EQ(rhythm(measureAt(score, 0)), "4 r8 4 4 4");
            EXPECT_EQ(rhythm(measureAt(score, 0), 1), "r4 8 r8 r2 r8");
            EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(9, 8));
        }

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertShorterReplaces)
{
    // eighth, triplet, 16th, triplet: in Insert input, a tuplet isn't nested in a waiting one, it replaces it
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    is.setDuration(DurationType::V_16TH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(is.pendingTuplet()->nested.empty());
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[16 r16 r16] 4 4 4");

    delete score;
}

// past the end of the score, in Insert input, as pendingTupletInsertBeyondScore
static MasterScore* pendingTupletInsertBeyondScoreScore()
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    is.setSegment(score->lastMeasure()->first(SegmentType::ChordRest));
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    score->select(toChord(chordRestAt(score->lastMeasure(), 3))->upNote());
    return score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertBeyondScoreDoesNotFit)
{
    // past the end of the score, a half quintuplet doesn't fit in the measure the next note appends: it waits,
    // the note is refused, and nothing is appended; an eighth triplet then replaces it and is appended
    MasterScore* score = pendingTupletInsertBeyondScoreScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_TRUE(is.beyondScore());
    const int measures = score->nmeasures();

    is.setDuration(DurationType::V_HALF);
    ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
    insertLetter(score);
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
    MScore::setError(MsError::MS_NO_ERROR);
    EXPECT_EQ(score->nmeasures(), measures);
    EXPECT_TRUE(is.pendingTupletHere());

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));
    insertLetter(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(score->nmeasures(), measures + 1);
    EXPECT_EQ(rhythm(score->lastMeasure()), "3:2[8 r8 r8] r4 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletInsertFlagDoesNotFit)
{
    // Ctrl+Shift+letter in By note name input, where the waiting tuplet doesn't fit: refused, as a letter is
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setDuration(DurationType::V_HALF);
    enterNote(score);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score);
    // Ctrl+Shift+letter needs a selected note
    score->select(toChord(chordRestAt(measureAt(score, 0), 1))->upNote());
    is.setSegment(score->tick2segment(Fraction(3, 4), false, SegmentType::ChordRest));

    ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
    insertLetter(score, true);
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
    MScore::setError(MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 r4");
    EXPECT_TRUE(is.pendingTupletHere());

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameDoesNotFitTabFret)
{
    // a fret on the linked tablature staff, where the waiting tuplet doesn't fit: refused on both staves
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"guitarLinkedTab.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(VOICES);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    is.setString(2);
    auto enterFret = [score](int fret) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::addFret(tx, score, fret);
        });
    };
    // on the whole note in the first measure, a half quintuplet is longer than the measure
    is.setDuration(DurationType::V_HALF);
    ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
    enterFret(5);
    EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
    MScore::setError(MsError::MS_NO_ERROR);
    EXPECT_TRUE(score->sanityCheck());
    // the whole note already there
    EXPECT_EQ(rhythm(score->firstMeasure()), "1");
    EXPECT_EQ(rhythm(score->firstMeasure(), VOICES), "1");
    EXPECT_TRUE(is.pendingTupletHere());

    delete score;
}

//-----------------------------------------
//    pending tuplets in Insert input with dotted durations:
//     single and double dots, before and after the tuplet
//     command
//-----------------------------------------

TEST_F(Engraving_TupletTests, pendingTupletInsertDottedNoteValue)
{
    // a dotted or double-dotted eighth triplet: its time (two of its notes) is inserted as rests, then the tuplet
    for (int dots : { 1, 2 }) {
        for (bool rest : { false, true }) {
            MasterScore* score = pendingTupletInsertScore();
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            TDuration eighth(DurationType::V_EIGHTH);
            eighth.setDots(dots);
            is.setDuration(eighth);
            ASSERT_TRUE(waitForTuplet(score, true));
            if (rest) {
                enterRest(score);
            } else {
                insertLetter(score);
            }
            EXPECT_TRUE(score->sanityCheck());
            const std::string r = rest ? "r" : "";
            EXPECT_EQ(rhythm(measureAt(score, 0)),
                      dots == 1 ? "4 3:2[" + r + "8. r8. r8.] 4 4 4" : "4 3:2[" + r + "8.. r8.. r8..] 4 4 4");
            EXPECT_EQ(measureAt(score, 0)->ticks(), dots == 1 ? Fraction(11, 8) : Fraction(23, 16));
            EXPECT_EQ(is.duration().dots(), dots);

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDotAfterCommand)
{
    // eighth, triplet, then a dot: the first note is dotted, in the eighth triplet, whose quarter is inserted
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        pressDot(score, dots);
        EXPECT_TRUE(is.pendingTupletHere());
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "4 3:2[8. r16 r8] 4 4 4" : "4 3:2[8.. r32 r8] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

        // the next note fills the triplet's rest
        pressDuration(score, dots == 1 ? DurationType::V_16TH : DurationType::V_32ND);
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "4 3:2[8. 16 r8] 4 4 4" : "4 3:2[8.. 32 r8] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDottedDivide)
{
    // triplet, dotted half: three quarters (3:3); triplet, double-dotted half: three eighths in the time of 7
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        TDuration half(DurationType::V_HALF);
        half.setDots(dots);
        is.setDuration(half);
        ASSERT_TRUE(waitForTuplet(score, false));
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "4 3:3[4 r4 r4] 4 4 4" : "4 3:7[8 r8 r8] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), dots == 1 ? Fraction(7, 4) : Fraction(15, 8));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDottedLongerNote)
{
    // eighth, triplet, then a dotted or double-dotted half: the note ends with the triplet
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        pressDuration(score, DurationType::V_HALF);
        pressDot(score, dots);
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4.] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
        expectD(chordRestAt(measureAt(score, 0), 2));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDottedFillingNote)
{
    // a dotted eighth triplet, then a later note longer than the two dotted eighths left: it ends with the triplet
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletInsertScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(1);
        is.setDuration(eighth);
        ASSERT_TRUE(waitForTuplet(score, true));
        insertLetter(score);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8. r8. r8.] 4 4 4");
        pressDuration(score, dots == 1 ? DurationType::V_HALF : DurationType::V_QUARTER);
        pressDot(score, dots);
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8. 4.] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(11, 8));
        expectD(chordRestAt(measureAt(score, 0), 3));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletInsertDottedBeyondScoreDoesNotFit)
{
    // past the end of the score, a dotted or double-dotted quarter quintuplet doesn't fit in a new measure: refused;
    // a dotted or double-dotted eighth triplet then replaces it and is appended
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletInsertBeyondScoreScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        const int measures = score->nmeasures();

        TDuration quarter(DurationType::V_QUARTER);
        quarter.setDots(dots);
        is.setDuration(quarter);
        ASSERT_TRUE(waitForTuplet(score, true, Fraction(5, 4)));
        EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
        insertLetter(score);
        EXPECT_EQ(MScore::_error, MsError::NO_SPACE_FOR_PENDING_TUPLET);
        MScore::setError(MsError::MS_NO_ERROR);
        EXPECT_EQ(score->nmeasures(), measures);

        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        is.setDuration(eighth);
        ASSERT_TRUE(waitForTuplet(score, true));
        insertLetter(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(score->nmeasures(), measures + 1);
        EXPECT_EQ(rhythm(score->lastMeasure()), dots == 1 ? "3:2[8. r8. r8.] r8 r2" : "3:2[8.. r8.. r8..] r16 r2");

        delete score;
    }
}

//-----------------------------------------
//    tupletPreview in By note name and Insert input
//     the tuplet waiting for a note, whose length is known
//-----------------------------------------

TEST_F(Engraving_TupletTests, tupletPreviewPendingNoteName)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // in By note name input, an eighth triplet: its length is known while it waits
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->levels.size(), 1u);
    EXPECT_EQ(preview->levels.back().ticks, Fraction(1, 4));
    EXPECT_EQ(preview->space, Fraction(1, 1));
    EXPECT_TRUE(preview->fits);
    EXPECT_FALSE(preview->waiting);
    EXPECT_FALSE(preview->inserts);

    // another duration is for the first note: the tuplet stays the same
    is.setDuration(DurationType::V_HALF);
    preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_EQ(preview->levels.back().ticks, Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingDoesNotFit)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // on beat 4, a quarter triplet would cross the barline
    is.setDuration(DurationType::V_HALF);
    enterNote(score);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score);
    ASSERT_TRUE(waitForTuplet(score, true));
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_EQ(preview->tick, Fraction(3, 4));
    EXPECT_EQ(preview->levels.back().ticks, Fraction(1, 2));
    EXPECT_EQ(preview->space, Fraction(1, 4));
    EXPECT_FALSE(preview->fits);

    // an eighth triplet fits
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_TRUE(preview->fits);

    delete score;
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingNumberAndBracket)
{
    // the waiting tuplet's number and bracket types, for the number in parentheses and the dashed bracket
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    score->inputState().setDuration(DurationType::V_EIGHTH);
    PendingTuplet pending;
    pending.ratio = Fraction(3, 2);
    pending.numberType = TupletNumberType::NO_TEXT;
    pending.bracketType = TupletBracketType::SHOW_NO_BRACKET;
    pending.durationAsSubdivision = true;
    ASSERT_TRUE(NoteInput::setPendingTuplet(score, pending));
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->levels.size(), 1u);
    EXPECT_EQ(preview->levels.back().numberType, TupletNumberType::NO_TEXT);
    EXPECT_EQ(preview->levels.back().bracketType, TupletBracketType::SHOW_NO_BRACKET);

    delete score;
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingInsert)
{
    // in Insert input, its time is inserted, so it is drawn over the music after it
    MasterScore* score = pendingTupletInsertScore();
    ASSERT_TRUE(score);
    score->inputState().setDuration(DurationType::V_HALF);
    ASSERT_TRUE(waitForTuplet(score, true));
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_TRUE(preview->inserts);
    EXPECT_TRUE(preview->fits);
    EXPECT_EQ(preview->levels.back().ticks, Fraction(1, 1));
    delete score;

    // past the end of the score, it goes in the measure that the next note appends
    score = pendingTupletInsertBeyondScoreScore();
    ASSERT_TRUE(score);
    score->inputState().setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_FALSE(preview->inserts);
    EXPECT_EQ(preview->tick, score->endTick());
    EXPECT_EQ(preview->space, Fraction(1, 1));
    delete score;
}

//-----------------------------------------
//    tuplets nested in a waiting tuplet in By note name
//     input: a tuplet command with a shorter duration than
//     the waiting tuplet's note value nests a tuplet in its
//     first note
//-----------------------------------------

// quarter, triplet, eighth, triplet: an eighth triplet in the first quarter of a quarter triplet
static MasterScore* nestedPendingTupletScore(const TDuration& outer = TDuration(DurationType::V_QUARTER),
                                             const TDuration& inner = TDuration(DurationType::V_EIGHTH))
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    is.setDuration(outer);
    if (!waitForTuplet(score, true)) {
        delete score;
        return nullptr;
    }
    is.setDuration(inner);
    if (!waitForTuplet(score, true)) {
        delete score;
        return nullptr;
    }
    return score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNested)
{
    MasterScore* score = nestedPendingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_TRUE(is.pendingTupletHere());
    EXPECT_EQ(is.pendingTuplet()->nested.size(), 1u);
    EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");

    // C D E: both tuplets are added with the first note
    for (int pitch : { 60, 62, 64 }) {
        enterNote(score, pitch);
    }
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 8 8] r4 r4] r2");
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(is.tick(), Fraction(1, 6));
    const ChordRest* first = chordRestAt(measureAt(score, 0), 0);
    ASSERT_TRUE(first && first->tuplet() && first->tuplet()->tuplet());
    EXPECT_EQ(first->tuplet()->baseLen().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(first->tuplet()->tuplet()->baseLen().type(), DurationType::V_QUARTER);

    // undoing the first note takes both tuplets away, and both wait again
    for (int i = 0; i < 3; ++i) {
        score->undoRedo(true, nullptr);
    }
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    ASSERT_TRUE(is.pendingTupletHere());
    EXPECT_EQ(is.pendingTuplet()->nested.size(), 1u);

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletCancelOneLevel)
{
    MasterScore* score = nestedPendingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_EQ(is.pendingTuplet()->nested.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

    // only the inner tuplet goes; the outer one still waits, with its note value
    NoteInput::cancelPendingTuplet(score);
    ASSERT_TRUE(is.pendingTupletHere());
    EXPECT_TRUE(is.pendingTuplet()->nested.empty());
    EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    EXPECT_EQ(preview->levels.size(), 1u);

    NoteInput::cancelPendingTuplet(score);
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletCancelOneLevelDotted)
{
    for (int dots : { 1, 2 }) {
        TDuration quarter(DurationType::V_QUARTER);
        quarter.setDots(dots);
        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        MasterScore* score = nestedPendingTupletScore(quarter, eighth);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        ASSERT_EQ(is.pendingTuplet()->nested.size(), 1u);
        EXPECT_EQ(is.duration(), eighth);

        NoteInput::cancelPendingTuplet(score);
        ASSERT_TRUE(is.pendingTupletHere());
        EXPECT_EQ(is.duration(), quarter);

        // the outer one is entered as if the inner one never waited
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[4. r4. r4.] r4" : "3:2[4.. r4.. r4..] r8");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedAgain)
{
    // with two tuplets waiting, a tuplet command that isn't shorter replaces the inner one only
    struct Case {
        DurationType changeTo;
        Fraction ratio;
        bool fits;
        std::string expected;
    };
    const std::vector<Case> cases {
        // quintuplet straight away: an eighth quintuplet in the quarter triplet
        { DurationType::V_INVALID, Fraction(5, 4), true, "3:2[ 5:4[8 r8 r8 r8 r8] r4] r2" },
        // quarter, triplet: a quarter triplet in the quarter triplet
        { DurationType::V_QUARTER, Fraction(3, 2), true, "3:2[ 3:2[4 r4 r4] r4] r2" },
        // half, triplet: a half triplet doesn't fit in the quarter triplet
        { DurationType::V_HALF, Fraction(3, 2), false, "R" },
    };
    for (const Case& c : cases) {
        MasterScore* score = nestedPendingTupletScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        if (c.changeTo != DurationType::V_INVALID) {
            pressDuration(score, c.changeTo);
        }
        ASSERT_TRUE(waitForTuplet(score, true, c.ratio));
        ASSERT_TRUE(is.pendingTupletHere());
        EXPECT_EQ(is.pendingTuplet()->nested.size(), 1u);
        EXPECT_EQ(NoteInput::pendingTupletFitsHere(score), c.fits);
        enterNote(score);
        EXPECT_EQ(MScore::_error, c.fits ? MsError::MS_NO_ERROR : MsError::NO_SPACE_FOR_PENDING_TUPLET);
        MScore::setError(MsError::MS_NO_ERROR);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), c.expected);

        if (!c.fits) {
            // an eighth and the tuplet command again replace the inner one with one that fits
            pressDuration(score, DurationType::V_EIGHTH);
            ASSERT_TRUE(waitForTuplet(score, true));
            EXPECT_EQ(is.pendingTuplet()->nested.size(), 1u);
            EXPECT_TRUE(NoteInput::pendingTupletFitsHere(score));
            enterNote(score);
            EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 r8 r8] r4 r4] r2");
        }

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedInNestedTuplet)
{
    // with two tuplets waiting, a 16th and the tuplet command nest a third one in the inner tuplet
    MasterScore* score = nestedPendingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    pressDuration(score, DurationType::V_16TH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_EQ(is.pendingTuplet()->nested.size(), 2u);
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[ 3:2[16 r16 r16] r8 r8] r4 r4] r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedOuterDoesNotFit)
{
    // a quarter triplet on beat 4 doesn't fit: a shorter duration and the tuplet command replace it, rather than nest
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setDuration(DurationType::V_HALF);
    enterNote(score);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_FALSE(NoteInput::pendingTupletFitsHere(score));
    pressDuration(score, DurationType::V_EIGHTH);
    ASSERT_TRUE(waitForTuplet(score, true));
    EXPECT_TRUE(is.pendingTuplet()->nested.empty());
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "2 4 3:2[8 r8 r8]");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedDivide)
{
    // the duration divided: quarter, triplet (three eighths), 16th, triplet (three 32nds in the time of a 16th)
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_TRUE(waitForTuplet(score, false));
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    is.setDuration(DurationType::V_16TH);
    ASSERT_TRUE(waitForTuplet(score, false));
    EXPECT_EQ(is.pendingTuplet()->nested.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_32ND);
    enterNote(score);
    EXPECT_TRUE(score->sanityCheck());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[32 r32 r32] r16 r8 r8] r4 r2");

    delete score;
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedEntryPaths)
{
    // after a quarter C, two tuplets waiting on beat 2 take a note entered each way
    const std::vector<std::pair<std::function<void(Score*)>, std::string> > paths {
        // letter
        { [](Score* score) {
                score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                    NoteInput::addPitch(tx, score, NoteInputParams { 0 }, false, false);
                });
            }, "4 3:2[ 3:2[8 r8 r8] r4 r4] r4" },
        // MIDI
        { [](Score* score) { enterNote(score); }, "4 3:2[ 3:2[8 r8 r8] r4 r4] r4" },
        // mouse, as for a tablature fret
        { [](Score* score) {
                Position pos;
                pos.segment = score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest);
                pos.staffIdx = 0;
                pos.line = 4;
                score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
                    NoteInput::putNote(tx, score, pos, false);
                });
            }, "4 3:2[ 3:2[8 r8 r8] r4 r4] r4" },
        // rest
        { [](Score* score) { enterRest(score); }, "4 3:2[ 3:2[r8 r8 r8] r4 r4] r4" },
        // R
        { [](Score* score) { repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 0))); }, "4 3:2[ 3:2[8 r8 r8] r4 r4] r4" },
        // tie
        { [](Score* score) { EditTie::cmdAddTie(score); }, "4~ 3:2[ 3:2[8 r8 r8] r4 r4] r4" },
        // Ctrl+Shift+letter: the outer tuplet's time is inserted
        { [](Score* score) { insertLetter(score, true); }, "4 3:2[ 3:2[8 r8 r8] r4 r4] r4 r2" },
    };
    for (size_t i = 0; i < paths.size(); ++i) {
        SCOPED_TRACE(i);
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        is.setDuration(DurationType::V_QUARTER);
        enterNote(score);
        // Ctrl+Shift+letter needs a selected note
        score->select(toChord(chordRestAt(measureAt(score, 0), 0))->upNote());
        is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        ASSERT_TRUE(waitForTuplet(score, true));
        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        ASSERT_EQ(is.pendingTuplet()->nested.size(), 1u);

        paths[i].first(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        MScore::setError(MsError::MS_NO_ERROR);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), paths[i].second);
        EXPECT_FALSE(is.pendingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedDrums)
{
    // a snare (voice 1) takes both tuplets; a bass drum (voice 2) cancels them
    for (bool bassDrum : { false, true }) {
        MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"tupletDrums.mscx");
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        is.setTrack(0);
        is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
        is.setNoteEntryMode(true);
        is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
        is.setDuration(DurationType::V_QUARTER);
        ASSERT_TRUE(waitForTuplet(score, true));
        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(waitForTuplet(score, true));
        score->transactionManager()->transaction(TranslatableString::untranslatable("Pending tuplet tests"), [&](Transaction& tx) {
            NoteInput::addPitch(tx, score, NoteInputParams { 0, bassDrum ? 36 : 38 }, false, false);
        });
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_FALSE(is.pendingTuplet());
        EXPECT_EQ(rhythm(measureAt(score, 0)), bassDrum ? "R" : "3:2[ 3:2[8 r8 r8] r4 r4] r2");

        delete score;
    }
}

//-----------------------------------------
//    tuplets nested in a waiting tuplet in By note name
//     input with dotted durations
//-----------------------------------------

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedDotted)
{
    // a dotted or double-dotted eighth triplet in the first note of a dotted or double-dotted quarter triplet
    for (int dots : { 1, 2 }) {
        TDuration quarter(DurationType::V_QUARTER);
        quarter.setDots(dots);
        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots);
        MasterScore* score = nestedPendingTupletScore(quarter, eighth);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        ASSERT_EQ(is.pendingTuplet()->nested.size(), 1u);
        EXPECT_EQ(is.duration(), eighth);
        for (int pitch : { 60, 62, 64 }) {
            enterNote(score, pitch);
        }
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[ 3:2[8. 8. 8.] r4. r4.] r4" : "3:2[ 3:2[8.. 8.. 8..] r4.. r4..] r8");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, pendingTupletNoteNameNestedDottedNotShorter)
{
    // a dotted or double-dotted eighth triplet waits; the same duration and the tuplet command replace it, and so does
    // a longer one: a dotted eighth isn't shorter than an eighth, nor a double-dotted eighth than a dotted one
    for (int dots : { 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        TDuration eighth(DurationType::V_EIGHTH);
        eighth.setDots(dots - 1);
        is.setDuration(eighth);
        ASSERT_TRUE(waitForTuplet(score, true));
        TDuration dotted(DurationType::V_EIGHTH);
        dotted.setDots(dots);
        is.setDuration(dotted);
        ASSERT_TRUE(waitForTuplet(score, true));
        EXPECT_TRUE(is.pendingTuplet()->nested.empty());
        enterNote(score);
        EXPECT_TRUE(score->sanityCheck());
        EXPECT_EQ(rhythm(measureAt(score, 0)), dots == 1 ? "3:2[8. r8. r8.] r8 r2" : "3:2[8.. r8.. r8..] r16 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, tupletPreviewPendingNested)
{
    // the eighth triplet waiting in the quarter triplet: two levels, from the outermost in, in score time
    MasterScore* score = nestedPendingTupletScore();
    ASSERT_TRUE(score);
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->levels.size(), 2u);
    EXPECT_EQ(preview->levels[0].ticks, Fraction(1, 2));
    EXPECT_EQ(preview->levels[1].ticks, Fraction(1, 6));
    EXPECT_EQ(preview->levels[1].ratio, Fraction(3, 2));
    EXPECT_TRUE(preview->fits);

    // a half triplet doesn't fit in it
    pressDuration(score, DurationType::V_HALF);
    ASSERT_TRUE(waitForTuplet(score, true));
    preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->levels.size(), 2u);
    EXPECT_FALSE(preview->fits);

    delete score;
}

//-----------------------------------------
//    continuing tuplets
//     note input keeps creating eighth note triplets,
//     in an empty 4/4 score
//-----------------------------------------

static const ContinuingTuplet::Level EIGHTH_TRIPLET { Fraction(3, 2), TDuration(DurationType::V_EIGHTH), TupletNumberType::SHOW_NUMBER,
                                                      TupletBracketType::AUTO_BRACKET };
static const ContinuingTuplet::Level QUARTER_TRIPLET { Fraction(3, 2), TDuration(DurationType::V_QUARTER), TupletNumberType::SHOW_NUMBER,
                                                       TupletBracketType::AUTO_BRACKET };

static ChordRest* continuingCrAt(Score* score, const Fraction& tick, track_idx_t track = 0)
{
    Segment* s = score->tick2segment(tick, false, SegmentType::ChordRest);
    return s ? toChordRest(s->element(track)) : nullptr;
}

// like a tuplet command with Continue tuplets on: a triplet at the input position, then continuing input
static void startContinuingTuplet(Score* score, const Fraction& tick, track_idx_t track = 0)
{
    const Tuplet* tuplet = nullptr;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
        tuplet = score->addTuplet(continuingCrAt(score, tick, track), EIGHTH_TRIPLET.ratio, EIGHTH_TRIPLET.baseLen,
                                  EIGHTH_TRIPLET.numberType, EIGHTH_TRIPLET.bracketType);
    });
    InputState& is = score->inputState();
    is.setSegment(score->tick2segment(tick, false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.setContinuingTuplet(ContinuingTuplet::of(tuplet));
}

static MasterScore* continuingTupletScore(NoteEntryMethod method = NoteEntryMethod::BY_NOTE_NAME, bool start = true)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"empty.mscx");
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(method);
    if (start) {
        startContinuingTuplet(score, Fraction(0, 1));
    }
    return score;
}

// note input in a score from tuplet_data, at the start of the given track
static MasterScore* continuingTupletScoreFrom(const String& file, track_idx_t track)
{
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + file);
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    is.setTrack(track);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    return score;
}

static void expectEighthTriplet(Tuplet* tuplet, const Fraction& tick)
{
    ASSERT_TRUE(tuplet);
    EXPECT_EQ(tuplet->tick(), tick);
    EXPECT_EQ(tuplet->ratio(), Fraction(3, 2));
    EXPECT_EQ(tuplet->baseLen().type(), DurationType::V_EIGHTH);
}

static void expectValid(MasterScore* score)
{
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
}

// like a triplet command in By duration or Rhythm input with Continue tuplets on
static void setPendingContinuingTriplet(Score* score)
{
    setPendingTriplet(score, true);
    InputState& is = score->inputState();
    PendingTuplet pending = *is.pendingTuplet();
    pending.continuing = true;
    is.setPendingTuplet(pending);
}

// a note or rest as each input method enters it: by note name, the duration and then the note or rest;
// by duration and rhythm, the dots and then the duration
static void enterDuration(Score* score, DurationType type, int dots = 0, bool rest = false)
{
    InputState& is = score->inputState();
    if (is.usingNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME)) {
        TDuration duration(type);
        duration.setDots(dots);
        is.setDuration(duration);
        if (rest) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
                score->enterRest(duration);
            });
        } else {
            enterNote(score);
        }
        return;
    }
    is.setDots(dots);
    is.setRest(rest);
    pressDuration(score, type);
    is.setRest(false);
}

TEST_F(Engraving_TupletTests, continuingTupletNotes)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // 7 notes: two full triplets and the start of a third
    for (int i = 0; i < 7; ++i) {
        enterNote(score, 60 + i);
    }
    expectValid(score);

    for (int i = 0; i < 3; ++i) {
        const Fraction tick = Fraction(i, 4);
        ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr);
        ASSERT_TRUE(cr->tuplet());
        expectEighthTriplet(cr->tuplet(), tick);
    }
    for (DurationElement* e : continuingCrAt(score, Fraction(1, 4))->tuplet()->elements()) {
        EXPECT_TRUE(e->isChord());
    }
    EXPECT_TRUE(score->inputState().continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletAcrossBarline)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // 4 triplets fill the bar, the 13th note starts a triplet in the next bar
    for (int i = 0; i < 13; ++i) {
        enterNote(score);
    }
    expectValid(score);
    ChordRest* cr = continuingCrAt(score, Fraction(1, 1));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isChord());
    ASSERT_TRUE(cr->tuplet());
    expectEighthTriplet(cr->tuplet(), Fraction(1, 1));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletRest)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // a triplet of notes, then a rest starts the next triplet
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
        score->enterRest(TDuration(DurationType::V_EIGHTH));
    });
    expectValid(score);

    ChordRest* rest = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(rest);
    EXPECT_TRUE(rest->isRest());
    ASSERT_TRUE(rest->tuplet());
    expectEighthTriplet(rest->tuplet(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletRestByDuration)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_DURATION);
    ASSERT_TRUE(score);

    // in input by duration mode, rests are entered by choosing a duration
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    score->inputState().setRest(true);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        NoteInput::setDuration(tx, score, DurationType::V_EIGHTH);
    });
    expectValid(score);

    ChordRest* rest = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(rest);
    EXPECT_TRUE(rest->isRest());
    ASSERT_TRUE(rest->tuplet());
    expectEighthTriplet(rest->tuplet(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletPending)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a tuplet waiting for the next duration starts continuing tuplet input when it is added
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 r8 r8] r2");
        EXPECT_TRUE(is.continuingTuplet());

        // undoing the first note ends continuing tuplet input, and the tuplet waits again
        for (int i = 0; i < 4; ++i) {
            score->undoRedo(true, nullptr);
        }
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        EXPECT_TRUE(is.pendingTupletHere());
        EXPECT_FALSE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletPendingUndoAfterStop)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // after continuing tuplet input stops (without forgetting the waiting tuplets), undoing the first note
        // brings back the waiting tuplet, which starts continuing tuplet input again
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        is.stopContinuingTuplet();
        for (int i = 0; i < 4; ++i) {
            score->undoRedo(true, nullptr);
        }
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        ASSERT_TRUE(is.pendingTupletHere());
        EXPECT_TRUE(is.pendingTuplet()->continuing);

        // the triplet is added, then the next one
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        expectValid(score);
        EXPECT_TRUE(is.continuingTuplet());
        ChordRest* cr = score->findCR(Fraction(1, 4), 0);
        ASSERT_TRUE(cr);
        EXPECT_TRUE(cr->isChord());
        ASSERT_TRUE(cr->tuplet());
        expectEighthTriplet(cr->tuplet(), Fraction(1, 4));

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletPendingUndoAfterEscape)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // after Escape ends continuing tuplet input, undoing the last note brings it back, but undoing everything
        // brings back neither the waiting tuplet nor continuing tuplet input
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        is.forgetPendingTuplets();
        is.stopContinuingTuplet();
        score->undoRedo(true, nullptr);
        EXPECT_TRUE(is.continuingTuplet());
        for (int i = 0; i < 3; ++i) {
            score->undoRedo(true, nullptr);
        }
        EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
        EXPECT_FALSE(is.pendingTuplet());
        EXPECT_FALSE(is.continuingTuplet());

        // the next notes are ordinary eighths
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 r2");

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletPendingNoteName)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // in By note name input, a waiting tuplet starts continuing tuplet input when the next note adds it
    is.setDuration(DurationType::V_EIGHTH);
    PendingTuplet pending;
    pending.ratio = Fraction(3, 2);
    pending.numberType = TupletNumberType::SHOW_NUMBER;
    pending.bracketType = TupletBracketType::AUTO_BRACKET;
    pending.autoBaseLen = true;
    pending.durationAsSubdivision = true;
    pending.continuing = true;
    ASSERT_TRUE(NoteInput::setPendingTuplet(score, pending));
    for (int i = 0; i < 4; ++i) {
        enterNote(score);
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 r8 r8] r2");
    EXPECT_TRUE(is.continuingTuplet());

    // undoing the first note ends continuing tuplet input, and the tuplet waits again
    for (int i = 0; i < 4; ++i) {
        score->undoRedo(true, nullptr);
    }
    EXPECT_EQ(rhythm(measureAt(score, 0)), "R");
    EXPECT_TRUE(is.pendingTupletHere());
    EXPECT_FALSE(is.continuingTuplet());

    delete score;
}

// like a tuplet command in By note name input with Continue tuplets on
static bool setPendingContinuingTupletNoteName(Score* score, const Fraction& ratio)
{
    PendingTuplet pending;
    pending.ratio = ratio;
    pending.numberType = TupletNumberType::SHOW_NUMBER;
    pending.bracketType = TupletBracketType::AUTO_BRACKET;
    pending.autoBaseLen = true;
    pending.durationAsSubdivision = true;
    pending.continuing = true;
    return NoteInput::setPendingTuplet(score, pending);
}

TEST_F(Engraving_TupletTests, continuingTupletBoundaryNests)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a run of quarter triplets; at its boundary, a shorter tuplet nests in the next quarter triplet
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    EXPECT_EQ(NoteInput::pendingTupletAsAdded(score).nested.size(), 1u);
    std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->levels.size(), 2u);
    EXPECT_EQ(preview->levels[0].ticks, Fraction(1, 2));
    EXPECT_EQ(preview->levels[1].ticks, Fraction(1, 6));
    EXPECT_TRUE(preview->fits);

    // the run carries on with both levels
    for (int i = 0; i < 4; ++i) {
        enterNote(score);
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] 3:2[ 3:2[8 8 8] 3:2[8 r8 r8] r4]");
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 2u);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletBoundaryReplaces)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // at the boundary of a run of quarter triplets, on bar 2, a quarter quintuplet replaces the continuing tuplet
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    for (int i = 0; i < 6; ++i) {
        enterNote(score);
    }
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(5, 4)));
    EXPECT_TRUE(NoteInput::pendingTupletAsAdded(score).nested.empty());
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 1)), "5:4[4 r4 r4 r4 r4]");
    ASSERT_TRUE(is.continuingTuplet());
    ASSERT_EQ(is.continuingTuplet()->levels.size(), 1u);
    EXPECT_EQ(is.continuingTuplet()->levels[0].ratio, Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletCancelPendingRestoresDuration)
{
    // the same with plain, dotted and double-dotted quarter triplets
    for (int dots : { 0, 1, 2 }) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        TDuration quarter(DurationType::V_QUARTER);
        quarter.setDots(dots);
        is.setDuration(quarter);

        ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
        for (int i = 0; i < 3; ++i) {
            enterNote(score);
        }
        ASSERT_TRUE(is.continuingTuplet());

        // a shorter tuplet waits at the boundary; cancelling it returns to the run's note value
        is.setDuration(DurationType::V_EIGHTH);
        ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
        EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
        NoteInput::cancelPendingTuplet(score);
        EXPECT_FALSE(is.pendingTuplet());
        ASSERT_TRUE(is.continuingTuplet());
        EXPECT_EQ(is.duration(), quarter);

        // and so does cancelling a longer one
        is.setDuration(DurationType::V_HALF);
        ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
        NoteInput::cancelPendingTuplet(score);
        EXPECT_EQ(is.duration(), quarter);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletCancelPendingWithoutRunKeepsDuration)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    NoteInput::cancelPendingTuplet(score);
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_FALSE(is.continuingTuplet());
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletBoundaryNoRoom)
{
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // quarter triplets from beat 2: the next one doesn't fit before the barline, so a shorter tuplet doesn't nest
    enterNote(score);
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    EXPECT_TRUE(NoteInput::pendingTupletAsAdded(score).nested.empty());
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4 4 4] 3:2[8 r8 r8]");
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletBoundaryByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // the duration after the tuplet command only sets the size: a shorter one than the run's replaces it too
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_QUARTER);
        }
        setPendingContinuingTriplet(score);
        // the size isn't known before the duration: only the number
        std::optional<TupletPreview> preview = NoteInput::tupletPreview(score);
        ASSERT_TRUE(preview);
        EXPECT_EQ(preview->levels.size(), 1u);
        EXPECT_TRUE(preview->waiting);
        for (int i = 0; i < 4; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] 3:2[8 8 8] 3:2[8 r8 r8]");
        ASSERT_TRUE(is.continuingTuplet());
        EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

        // the same note value replaces it
        for (int i = 0; i < 4; ++i) {
            score->undoRedo(true, nullptr);
        }
        EXPECT_TRUE(is.pendingTupletHere());
        pressDuration(score, DurationType::V_QUARTER);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[4 4 4] 3:2[4 r4 r4]");
        ASSERT_TRUE(is.continuingTuplet());
        EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletOverwriteByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = continuingTupletScore(method, false);
        ASSERT_TRUE(score);

        // a bar of eighths, a continuing triplet on the first beat, then 4 eighths entered by duration:
        // the 4th replaces the eighth on the second beat with the next triplet instead of adding to it
        for (int i = 0; i < 8; ++i) {
            enterNote(score, 62);
        }
        startContinuingTuplet(score, Fraction(0, 1));
        score->inputState().setNotes({ NoteVal(60) });
        for (int i = 0; i < 4; ++i) {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
                NoteInput::setDuration(tx, score, DurationType::V_EIGHTH);
            });
        }
        expectValid(score);

        ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
        ASSERT_TRUE(cr && cr->isChord());
        EXPECT_EQ(toChord(cr)->notes().size(), 1u);
        ASSERT_TRUE(cr->tuplet());
        expectEighthTriplet(cr->tuplet(), Fraction(1, 4));
        EXPECT_TRUE(score->inputState().continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletSpillOver)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // an eighth, then a half note on the 2nd eighth of the triplet: a quarter at the end of this
    // triplet, tied to a quarter in the next one
    enterNote(score, 60);
    score->inputState().setDuration(DurationType::V_HALF);
    enterNote(score, 62);
    expectValid(score);

    Tuplet* first = continuingCrAt(score, Fraction(0, 1))->tuplet();
    ASSERT_TRUE(first);
    expectEighthTriplet(first, Fraction(0, 1));
    ASSERT_EQ(first->elements().size(), 2u);
    ASSERT_TRUE(first->elements().at(1)->isChord());
    Chord* tiedFrom = toChord(first->elements().at(1));
    EXPECT_EQ(tiedFrom->durationType().type(), DurationType::V_QUARTER);

    ChordRest* next = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(next);
    ASSERT_TRUE(next->isChord());
    ASSERT_TRUE(next->tuplet());
    expectEighthTriplet(next->tuplet(), Fraction(1, 4));
    EXPECT_EQ(next->durationType().type(), DurationType::V_QUARTER);
    ASSERT_TRUE(tiedFrom->upNote()->tieFor());
    EXPECT_EQ(tiedFrom->upNote()->tieFor()->endNote(), toChord(next)->upNote());
    // the rest of the second triplet is a rest
    ASSERT_EQ(next->tuplet()->elements().size(), 2u);
    EXPECT_TRUE(next->tuplet()->elements().at(1)->isRest());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoom)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // 5 ordinary eighths, a continuing triplet up to 7/8, then the next triplet doesn't fit
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    MScore::setError(MsError::MS_NO_ERROR);

    // the next note ends continuing tuplet input and goes in as an ordinary one
    EXPECT_FALSE(NoteInput::nextContinuingTupletFits(score, is));
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    ChordRest* last = continuingCrAt(score, Fraction(7, 8));
    ASSERT_TRUE(last);
    EXPECT_TRUE(last->isChord());
    EXPECT_FALSE(last->tuplet());
    EXPECT_FALSE(is.continuingTuplet());

    // undoing it brings continuing tuplet input back, and redoing it ends it again
    score->undoRedo(true, nullptr);
    EXPECT_TRUE(continuingCrAt(score, Fraction(7, 8))->isRest());
    EXPECT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.tick(), Fraction(7, 8));
    score->undoRedo(false, nullptr);
    EXPECT_TRUE(continuingCrAt(score, Fraction(7, 8))->isChord());
    EXPECT_FALSE(is.continuingTuplet());

    // a rest ends it the same way
    score->undoRedo(true, nullptr);
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
        score->enterRest(TDuration(DurationType::V_EIGHTH));
    });
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] r8");
    EXPECT_FALSE(is.continuingTuplet());
    EXPECT_EQ(is.tick(), Fraction(1, 1));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoomMouseAndTabFret)
{
    // where the next triplet doesn't fit, a note entered with the mouse or a fret on a tablature staff ends continuing tuplet
    // input and goes in as an ordinary one
    for (bool tab : { false, true }) {
        const track_idx_t track = tab ? VOICES : 0;
        MasterScore* score = tab ? continuingTupletScoreFrom(u"continuingTupletLinkedTab.mscx", track)
                             : continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        is.setString(2);
        auto enter = [&]() {
            score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
                if (tab) {
                    NoteInput::addFret(tx, score, 3);
                } else {
                    Position pos;
                    pos.segment = is.segment();
                    pos.staffIdx = 0;
                    pos.line = 4;
                    NoteInput::putNote(tx, score, pos, false);
                }
            });
            if (tab) {
                is.moveToNextInputPos();
            }
        };

        for (int i = 0; i < 5; ++i) {
            enter();
        }
        startContinuingTuplet(score, Fraction(5, 8), track);
        is.setTrack(track);
        for (int i = 0; i < 3; ++i) {
            enter();
        }
        ASSERT_EQ(rhythm(measureAt(score, 0), track), "8 8 8 8 8 3:2[8 8 8] r8");
        ASSERT_TRUE(is.continuingTuplet());
        MScore::setError(MsError::MS_NO_ERROR);
        enter();
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0), track), "8 8 8 8 8 3:2[8 8 8] 8");
        EXPECT_FALSE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoomChord)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // where the next triplet doesn't fit, a note can still be added to the chord just entered
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    MScore::setError(MsError::MS_NO_ERROR);
    enterNote(score, 64, true);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    const ChordRest* cr = score->inputState().lastSegment()->cr(0);
    ASSERT_TRUE(cr && cr->isChord());
    EXPECT_EQ(toChord(cr)->notes().size(), 2u);
    EXPECT_TRUE(score->inputState().continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoomSpillOver)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // in the last triplet that fits, a half note would continue in a triplet that doesn't: it ends continuing tuplet
    // input and goes in as without it
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    MScore::setError(MsError::MS_NO_ERROR);
    is.setDuration(DurationType::V_HALF);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[4.~] 8");
    EXPECT_FALSE(is.continuingTuplet());

    // after undo, a quarter fills the triplet
    score->undoRedo(true, nullptr);
    EXPECT_TRUE(is.continuingTuplet());
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[4 r8] r8");
    EXPECT_TRUE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoomByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // a quarter, then continuing quarter triplets from beat 2: the second one doesn't fit
        pressDuration(score, DurationType::V_QUARTER);
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_QUARTER);
        }
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4 4 4] r4");
        MScore::setError(MsError::MS_NO_ERROR);

        // a duration key ends continuing tuplet input and enters an ordinary eighth
        pressDuration(score, DurationType::V_EIGHTH);
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4 4 4] 8 r8");
        EXPECT_FALSE(is.continuingTuplet());

        // undoing it brings continuing tuplet input back; a rest then ends it the same way
        score->undoRedo(true, nullptr);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4 4 4] r4");
        EXPECT_TRUE(is.continuingTuplet());
        is.setRest(true);
        pressDuration(score, DurationType::V_EIGHTH);
        is.setRest(false);
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[4 4 4] r8 r8");
        EXPECT_FALSE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletMeasureRepeat)
{
    // whole-bar half-note triplets reach bar 2, which has a measure repeat: by entering a note there
    // (by duration and rhythm add the tuplet before the note), or by a note spilling over into it
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM, NoteEntryMethod::BY_NOTE_NAME }) {
        MasterScore* score = continuingTupletScore(method, false);
        ASSERT_TRUE(score);
        Measure* bar2 = score->tick2measure(Fraction(1, 1));
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            EditMeasureRepeat::addMeasureRepeat(tx, score, bar2, 1, 0);
        });
        ASSERT_TRUE(bar2->isMeasureRepeatGroup(0));

        const Tuplet* tuplet = nullptr;
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
            tuplet = score->addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_HALF),
                                      TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
        });
        ASSERT_TRUE(tuplet);
        InputState& is = score->inputState();
        is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
        is.setContinuingTuplet(ContinuingTuplet::of(tuplet));
        is.setNotes({ NoteVal(60) });

        // by note name, the third note is a whole note that spills over into bar 2
        const bool spillOver = method == NoteEntryMethod::BY_NOTE_NAME;
        for (int i = 0; i < (spillOver ? 3 : 4); ++i) {
            const DurationType d = (spillOver && i == 2) ? DurationType::V_WHOLE : DurationType::V_HALF;
            if (spillOver) {
                is.setDuration(d);
                enterNote(score);
            } else {
                score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"),
                                                         [&](Transaction& tx) {
                    NoteInput::setDuration(tx, score, d);
                });
            }
        }
        expectValid(score);

        // the measure repeat is gone, and the continuing tuplet is in bar 2
        EXPECT_FALSE(bar2->isMeasureRepeatGroup(0));
        ChordRest* cr = continuingCrAt(score, Fraction(1, 1));
        ASSERT_TRUE(cr);
        EXPECT_TRUE(cr->isChord());
        ASSERT_TRUE(cr->tuplet());
        EXPECT_EQ(cr->tuplet()->tick(), Fraction(1, 1));
        EXPECT_EQ(cr->tuplet()->ratio(), Fraction(3, 2));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletChord)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // adding a note to the chord doesn't create another tuplet
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 60);
    }
    enterNote(score, 62);
    enterNote(score, 66, true);
    enterNote(score, 64);
    expectValid(score);

    ChordRest* first = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(first && first->isChord());
    EXPECT_EQ(toChord(first)->notes().size(), 2u);
    Tuplet* tuplet = first->tuplet();
    ASSERT_TRUE(tuplet);
    expectEighthTriplet(tuplet, Fraction(1, 4));
    EXPECT_TRUE(tuplet->elements().at(1)->isChord());
    EXPECT_EQ(toChordRest(tuplet->elements().at(1))->tuplet(), tuplet);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletUndo)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // the next triplet and its first note are undone in one step
    for (int i = 0; i < 4; ++i) {
        enterNote(score);
    }
    ASSERT_TRUE(continuingCrAt(score, Fraction(1, 4))->tuplet());
    score->undoRedo(true, nullptr);
    expectValid(score);

    ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isRest());
    EXPECT_FALSE(cr->tuplet());
    EXPECT_TRUE(score->inputState().continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletUndoAfterStop)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // continuing tuplet input stopped after the last note, e.g. with Escape: undo brings it back with the input position in
    // its tuplets, and redo stops it again
    for (int i = 0; i < 4; ++i) {
        enterNote(score);
    }
    InputState& is = score->inputState();
    is.stopContinuingTuplet();
    score->undoRedo(true, nullptr);
    EXPECT_TRUE(is.continuingTuplet());
    score->undoRedo(false, nullptr);
    EXPECT_FALSE(is.continuingTuplet());
    score->undoRedo(true, nullptr);

    // the next note starts the next triplet again
    enterNote(score);
    expectValid(score);
    ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isChord());
    ASSERT_TRUE(cr->tuplet());
    expectEighthTriplet(cr->tuplet(), Fraction(1, 4));

    delete score;
}

// note input on beat 2 of a quarter and an eighth triplet of rests on beat 2, by any method
static MasterScore* tupletOnBeat2Score(NoteEntryMethod method)
{
    MasterScore* score = pendingTupletScore(method);
    if (!score) {
        return nullptr;
    }
    enterDuration(score, DurationType::V_QUARTER);
    ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
    if (!cr || !addTuplet(cr, Fraction(3, 2), TDuration(DurationType::V_EIGHTH))) {
        delete score;
        return nullptr;
    }
    score->inputState().setDuration(DurationType::V_EIGHTH);
    return score;
}

TEST_F(Engraving_TupletTests, continuingTupletStartInside)
{
    // note input that starts, or moves, inside a triplet goes on with continuing triplets: from its second or its last note
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int slot : { 1, 2 }) {
            MasterScore* score = tupletOnBeat2Score(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();
            ASSERT_EQ(rhythm(measureAt(score, 0)), "4 3:2[r8 r8 r8] r2");

            is.setSegment(score->tick2segment(Fraction(1, 4) + Fraction(slot, 12), false, SegmentType::ChordRest));
            EXPECT_TRUE(is.startContinuingTupletInside());
            ASSERT_TRUE(is.continuingTuplet());
            ASSERT_EQ(is.continuingTuplet()->levels.size(), 1u);
            EXPECT_EQ(is.continuingTuplet()->levels[0].ratio, Fraction(3, 2));
            EXPECT_EQ(is.continuingTuplet()->levels[0].baseLen.type(), DurationType::V_EIGHTH);

            // on already: nothing changes
            EXPECT_FALSE(is.startContinuingTupletInside());

            for (int i = slot; i < 3 + 3; ++i) {
                enterDuration(score, DurationType::V_EIGHTH);
            }
            expectValid(score);
            EXPECT_EQ(rhythm(measureAt(score, 0)), slot == 1 ? "4 3:2[r8 8 8] 3:2[8 8 8] r4" : "4 3:2[r8 r8 8] 3:2[8 8 8] r4");
            const ChordRest* next = continuingCrAt(score, Fraction(1, 2));
            ASSERT_TRUE(next);
            expectEighthTriplet(next->tuplet(), Fraction(1, 2));
            EXPECT_TRUE(is.continuingTuplet());

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, continuingTupletStartInsideRestAndMouse)
{
    // started inside the triplet: a rest fills it, and a note entered with the mouse straight after it starts the next
    MasterScore* score = tupletOnBeat2Score(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setSegment(score->tick2segment(Fraction(5, 12), false, SegmentType::ChordRest));
    ASSERT_TRUE(is.startContinuingTupletInside());
    enterRest(score);
    Position pos;
    pos.segment = score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest);
    pos.staffIdx = 0;
    pos.line = 4;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        NoteInput::putNote(tx, score, pos, false);
    });
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[r8 r8 r8] 3:2[8 r8 r8] r4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStartInsideNested)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a quarter triplet with an eighth triplet in its first note; input starts on the second note of that
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_EIGHTH)));
    ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[r8 r8 r8] r4 r4] r2");
    is.setSegment(score->tick2segment(Fraction(1, 18), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    ASSERT_TRUE(is.startContinuingTupletInside());
    ASSERT_EQ(is.continuingTuplet()->levels.size(), 2u);
    EXPECT_EQ(is.continuingTuplet()->levels[0].baseLen.type(), DurationType::V_QUARTER);
    EXPECT_EQ(is.continuingTuplet()->levels[1].baseLen.type(), DurationType::V_EIGHTH);

    // both levels go on, past the end of the quarter triplet
    for (int i = 0; i < 2 + 3 + 3 + 1; ++i) {
        enterNote(score);
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[r8 8 8] 3:2[8 8 8] 3:2[8 8 8]] 3:2[ 3:2[8 r8 r8] r4 r4]");

    // stopped in the second quarter triplet: no run starts again in any of its eighth triplets
    is.stopContinuingTuplet();
    is.setSegment(score->tick2segment(Fraction(1, 2) + Fraction(1, 6), false, SegmentType::ChordRest));
    EXPECT_FALSE(is.startContinuingTupletInside());
    is.setSegment(score->tick2segment(Fraction(1, 6), false, SegmentType::ChordRest));
    EXPECT_TRUE(is.startContinuingTupletInside());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStartInsideDotted)
{
    // a dotted or double-dotted eighth triplet: the run goes on with its note value
    for (int dots : { 1, 2 }) {
        MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        TDuration dotted(DurationType::V_EIGHTH);
        dotted.setDots(dots);
        ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), dotted));
        is.setSegment(score->tick2segment(dotted.fraction() * Fraction(2, 3), false, SegmentType::ChordRest));
        is.setDuration(dotted);
        ASSERT_TRUE(is.startContinuingTupletInside());
        EXPECT_EQ(is.continuingTuplet()->levels[0].baseLen, dotted);

        for (int i = 0; i < 2 + 3; ++i) {
            enterNote(score);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)),
                  dots == 1 ? "3:2[r8. 8. 8.] 3:2[8. 8. 8.] r4" : "3:2[r8.. 8.. 8..] 3:2[8.. 8.. 8..] r8");
        const ChordRest* second = continuingCrAt(score, dotted.fraction() * 2);
        ASSERT_TRUE(second && second->tuplet());
        EXPECT_EQ(second->tuplet()->baseLen(), dotted);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletStartInsideOnlyInside)
{
    MasterScore* score = tupletOnBeat2Score(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // not straight after the triplet, nor beyond the score
    is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
    EXPECT_FALSE(is.startContinuingTupletInside());
    is.setBeyondScore(true);
    EXPECT_FALSE(is.startContinuingTupletInside());
    is.setBeyondScore(false);

    // nor with input methods without continuing tuplets
    is.setSegment(score->tick2segment(Fraction(1, 3), false, SegmentType::ChordRest));
    for (NoteEntryMethod method : { NoteEntryMethod::REPITCH, NoteEntryMethod::REALTIME_AUTO, NoteEntryMethod::REALTIME_MANUAL }) {
        is.setNoteEntryMethod(method);
        EXPECT_FALSE(is.startContinuingTupletInside());
    }
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    EXPECT_TRUE(is.startContinuingTupletInside());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStoppedInside)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // stopped (e.g. with Escape) on the second note of the triplet: the next notes are ordinary ones, and no run starts
    // again in this triplet
    enterNote(score);
    is.stopContinuingTuplet();
    enterNote(score);
    EXPECT_FALSE(is.startContinuingTupletInside());
    enterNote(score);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 8 r8 r2");
    EXPECT_FALSE(is.continuingTuplet());

    // until the input position has left it
    EXPECT_FALSE(is.startContinuingTupletInside());
    is.setSegment(score->tick2segment(Fraction(1, 12), false, SegmentType::ChordRest));
    EXPECT_TRUE(is.startContinuingTupletInside());

    // stopped there again: starting note input again forgets it
    is.stopContinuingTuplet();
    EXPECT_FALSE(is.startContinuingTupletInside());
    is.setNoteEntryMode(false);
    is.setNoteEntryMode(true);
    EXPECT_TRUE(is.startContinuingTupletInside());

    delete score;
}

// continuing eighth triplets in a quarter triplet at the start of the measure
static MasterScore* continuingTupletNestedScore()
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    if (!score) {
        return nullptr;
    }
    addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER));
    startContinuingTuplet(score, Fraction(0, 1));
    return score;
}

// what moving the input position does to continuing tuplet input
static void moveContinuingInput(Score* score, const Fraction& tick)
{
    InputState& is = score->inputState();
    is.setSegment(score->tick2segment(tick, false, SegmentType::ChordRest));
    if (!is.continuingTupletGoesOn()) {
        is.setContinuingTuplet(std::nullopt);
    }
    is.startContinuingTupletInside();
}

TEST_F(Engraving_TupletTests, continuingTupletCancelPendingLeavesPartlyFilledDuration)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // stepped out of an inner triplet that isn't full yet: cancelling leaves the duration, to fill it
    enterNote(score);
    enterNote(score);
    is.stepOutOfContinuingTuplet();
    ASSERT_EQ(is.continuingTuplet()->levels.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    ASSERT_TRUE(setPendingContinuingTupletNoteName(score, Fraction(3, 2)));
    NoteInput::cancelPendingTuplet(score);
    EXPECT_FALSE(is.pendingTuplet());
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutKeepsOuterRun)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    ASSERT_EQ(is.continuingTuplet()->levels.size(), 2u);

    // the duration becomes the outer note value, and the next notes are straight notes of the outer run
    is.stepOutOfContinuingTuplet();
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);
    enterNote(score);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 8 8] 4 4] r2");

    // the next outer tuplet has one level
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 8 8] 4 4] 3:2[4 r4 r4]");
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

    // the last level stops the run, and no run starts again in this tuplet
    is.stepOutOfContinuingTuplet();
    EXPECT_FALSE(is.continuingTuplet());
    EXPECT_FALSE(is.startContinuingTupletInside());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutInPartlyFilledTuplet)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // the duration stays until the inner tuplet is full, and after it
    enterNote(score);
    enterNote(score);
    is.stepOutOfContinuingTuplet();
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    enterNote(score);
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 8 8] 8 r8 r4] r2");
    EXPECT_TRUE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutLongerNoteSpills)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a quarter on the last note of the inner tuplet goes on, tied, in the outer tuplet
    enterNote(score, 60);
    enterNote(score, 62);
    is.stepOutOfContinuingTuplet();
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score, 64);
    expectValid(score);
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);
    ChordRest* cr = continuingCrAt(score, Fraction(1, 6));
    ASSERT_TRUE(cr && cr->isChord());
    EXPECT_EQ(toChord(cr)->upNote()->pitch(), 64);
    EXPECT_TRUE(toChord(cr)->upNote()->tieBack());
    EXPECT_EQ(cr->tuplet()->baseLen().type(), DurationType::V_QUARTER);
    EXPECT_FALSE(cr->tuplet()->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutThreeLevels)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // eighth triplets in quarter triplets in a half triplet; the input is in the innermost tuplet
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_HALF)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_EIGHTH)));
    ASSERT_TRUE(is.startContinuingTupletInside());
    ASSERT_EQ(is.continuingTuplet()->levels.size(), 3u);
    enterNote(score);

    for (size_t levels : { 2u, 1u }) {
        is.stepOutOfContinuingTuplet();
        ASSERT_TRUE(is.continuingTuplet());
        EXPECT_EQ(is.continuingTuplet()->levels.size(), levels);
        EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);
        EXPECT_TRUE(is.continuingTupletGoesOn());
    }
    is.stepOutOfContinuingTuplet();
    EXPECT_FALSE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutThreeLevelsDuration)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // the first inner tuplets are full: each step gives the note value of the level left, until an inner tuplet is
    // only partly filled
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_HALF)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_EIGHTH)));
    ASSERT_TRUE(is.startContinuingTupletInside());
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.stepOutOfContinuingTuplet();
    EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);
    is.stepOutOfContinuingTuplet();
    EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutMoves)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.stepOutOfContinuingTuplet();

    // back into the inner tuplet just left: nesting stays off, and a note there doesn't end the run
    moveContinuingInput(score, Fraction(1, 9));
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);
    is.setDuration(DurationType::V_EIGHTH);
    enterNote(score, 67);
    expectValid(score);
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

    // a different nested tuplet: all its levels
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(1, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(1, 1)), Fraction(3, 2), TDuration(DurationType::V_EIGHTH)));
    moveContinuingInput(score, Fraction(1, 1));
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 2u);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutLeaveAndComeBack)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // the run goes on into a new outer tuplet of one level
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.stepOutOfContinuingTuplet();
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    moveContinuingInput(score, is.tick());
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

    // out of the tuplet and back in gives all levels
    moveContinuingInput(score, Fraction(1, 9));
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 2u);

    // and stepping out works again
    is.stepOutOfContinuingTuplet();
    moveContinuingInput(score, Fraction(1, 18));
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutUndo)
{
    MasterScore* score = continuingTupletNestedScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // C D E, Escape, F: the first undo takes F away and stays stepped out, the second takes E away and nests again
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    is.stepOutOfContinuingTuplet();
    enterNote(score);
    score->undoRedo(true, nullptr);
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 1u);
    EXPECT_EQ(is.duration().type(), DurationType::V_QUARTER);
    EXPECT_TRUE(is.continuingTupletGoesOn());
    score->undoRedo(true, nullptr);
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 2u);
    EXPECT_EQ(is.duration().type(), DurationType::V_EIGHTH);

    // C D E, Escape, undo: E goes, nesting back
    enterNote(score);
    is.stepOutOfContinuingTuplet();
    score->undoRedo(true, nullptr);
    ASSERT_TRUE(is.continuingTuplet());
    EXPECT_EQ(is.continuingTuplet()->levels.size(), 2u);
    enterNote(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[ 3:2[8 8 8] r4 r4] r2");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletStepOutDotted)
{
    // a dotted or double-dotted level: the duration gets its dots, and the next note goes straight in the outer tuplet
    for (int dots : { 1, 2 }) {
        MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        TDuration outerLen(DurationType::V_QUARTER);
        outerLen.setDots(dots);
        TDuration innerLen(DurationType::V_EIGHTH);
        innerLen.setDots(dots);
        ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), outerLen));
        ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), innerLen));
        is.setDuration(innerLen);
        ASSERT_TRUE(is.startContinuingTupletInside());
        for (int i = 0; i < 3; ++i) {
            enterNote(score);
        }

        is.stepOutOfContinuingTuplet();
        ASSERT_TRUE(is.continuingTuplet());
        EXPECT_EQ(is.duration(), outerLen);
        const Fraction tick = is.tick();
        enterNote(score);
        expectValid(score);
        ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr && cr->isChord() && cr->tuplet());
        EXPECT_EQ(cr->durationType(), outerLen);
        EXPECT_EQ(cr->tuplet()->baseLen(), outerLen);
        EXPECT_FALSE(cr->tuplet()->tuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletMouse)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // a triplet, then a note entered with the mouse straight after it starts the next triplet
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    Position pos;
    pos.segment = score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest);
    pos.staffIdx = 0;
    pos.line = 4;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        NoteInput::putNote(tx, score, pos, false);
    });
    expectValid(score);

    ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isChord());
    ASSERT_TRUE(cr->tuplet());
    expectEighthTriplet(cr->tuplet(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continueTupletsAllowed)
{
    InputState is;
    const std::vector<std::pair<NoteEntryMethod, bool> > methods {
        { NoteEntryMethod::BY_NOTE_NAME, true }, { NoteEntryMethod::BY_DURATION, true }, { NoteEntryMethod::RHYTHM, true },
        { NoteEntryMethod::REPITCH, false }, { NoteEntryMethod::REALTIME_AUTO, false }, { NoteEntryMethod::REALTIME_MANUAL, false },
        { NoteEntryMethod::TIMEWISE, true } };
    for (const auto& [method, allowed] : methods) {
        is.setNoteEntryMethod(method);
        EXPECT_EQ(is.continueTupletsAllowed(), allowed);
    }
}

TEST_F(Engraving_TupletTests, continuingTupletInsertMode)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // 5 eighths and a continuing triplet up to 7/8, where the next triplet doesn't fit
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }

    // in Insert mode, the run goes on: the next triplet's whole time is inserted, which lengthens the bar
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    score->select(continuingCrAt(score, Fraction(7, 8)));
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] 3:2[8 r8 r8] r8");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));
    EXPECT_TRUE(is.continuingTuplet());

    // the next notes fill it, then the next one inserts another triplet
    for (int i = 0; i < 3; ++i) {
        insertLetter(score);
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] 3:2[8 8 8] 3:2[8 r8 r8] r8");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(3, 2));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeRest)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a continuing triplet, then quarters C D E in Insert mode, back on C
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    const ContinuingTuplet continuing = *is.continuingTuplet();
    is.setContinuingTuplet(std::nullopt);
    is.setDuration(DurationType::V_QUARTER);
    for (int pitch : { 60, 62, 64 }) {
        enterNote(score, pitch);
    }
    ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 4 4 4");
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setContinuingTuplet(continuing);

    // a half rest inserts the next triplet's time, and fills the triplet without going past it
    is.setDuration(DurationType::V_HALF);
    enterRest(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[r4.] 4 4 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertFlag)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // a continuing triplet, then a quarter G without continuing tuplets
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 60 + i);
    }
    const ContinuingTuplet continuing = *is.continuingTuplet();
    is.setContinuingTuplet(std::nullopt);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score, 67);
    ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 4 r2");

    // back after the triplet, Ctrl+Shift+letter (insert) inserts the next continuing triplet's whole time: G stays a quarter
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setDuration(DurationType::V_EIGHTH);
    is.setContinuingTuplet(continuing);
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score, true);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 r8 r8] 4 r2");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

    // the next letters fill it as before
    enterNote(score, 62);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 8 r8] 4 r2");

    delete score;
}

// a continuing triplet, then quarters C D E, in Insert input on C with the eighth triplet run going on
static MasterScore* continuingTupletInsertScore()
{
    MasterScore* score = continuingTupletScore();
    if (!score) {
        return nullptr;
    }
    InputState& is = score->inputState();
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    const ContinuingTuplet continuing = *is.continuingTuplet();
    is.setContinuingTuplet(std::nullopt);
    is.setDuration(DurationType::V_QUARTER);
    for (int pitch : { 60, 62, 64 }) {
        enterNote(score, pitch);
    }
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setDuration(DurationType::V_EIGHTH);
    is.setContinuingTuplet(continuing);
    return score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeMidiAndRepeat)
{
    // a note from a MIDI keyboard, or R, inserts the next triplet's time as a letter does: C, D and E are kept
    for (bool repeat : { false, true }) {
        MasterScore* score = continuingTupletInsertScore();
        ASSERT_TRUE(score);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 4 4 4");
        MScore::setError(MsError::MS_NO_ERROR);
        if (repeat) {
            repeatChord(score, toChord(chordRestAt(measureAt(score, 0), 2)));
        } else {
            enterNote(score, 67);
        }
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 r8 r8] 4 4 4");
        EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(5, 4));

        // the next ones fill it
        enterNote(score, 67);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 8 r8] 4 4 4");
        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletStartInsideInsert)
{
    // Insert input started inside a triplet: straight after it, a note inserts the next triplet's time
    MasterScore* score = continuingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    ASSERT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 4 4 4");
    is.setContinuingTuplet(std::nullopt);
    is.setSegment(score->tick2segment(Fraction(1, 12), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    ASSERT_TRUE(is.startContinuingTupletInside());

    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    ASSERT_TRUE(is.continuingTupletGoesOn());
    MScore::setError(MsError::MS_NO_ERROR);
    enterNote(score, 67);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 r8 r8] 4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeMidiAtBarEnd)
{
    // on beat 4, where a quarter triplet's time doesn't fit before the barline, MIDI inserts it as a letter does
    MasterScore* score = pendingTupletInsertScore(Fraction(3, 4));
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setDuration(DurationType::V_QUARTER);
    is.resumeContinuingTuplet(ContinuingTuplet { { QUARTER_TRIPLET } });
    EXPECT_TRUE(NoteInput::nextContinuingTupletFits(score, is));
    MScore::setError(MsError::MS_NO_ERROR);
    enterNote(score, 67);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 3:2[4 r4 r4] 4");
    EXPECT_EQ(measureAt(score, 0)->ticks(), Fraction(3, 2));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeTabFret)
{
    // a fret on the tablature staff inserts the next triplet's time, on both staves
    MasterScore* score = continuingTupletScoreFrom(u"continuingTupletLinkedTab.mscx", VOICES);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setString(2);
    is.setDuration(DurationType::V_QUARTER);
    for (int i = 0; i < 4; ++i) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            NoteInput::addFret(tx, score, 3);
        });
        is.moveToNextInputPos();
    }
    ASSERT_EQ(rhythm(measureAt(score, 0), VOICES), "4 4 4 4");
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setTrack(VOICES);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.resumeContinuingTuplet(ContinuingTuplet { { EIGHTH_TRIPLET } });
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        NoteInput::addFret(tx, score, 5);
    });
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0), VOICES), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(rhythm(measureAt(score, 0), 0), "4 3:2[8 r8 r8] 4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertOtherVoiceTuplet)
{
    // a quarter triplet on beats 1-2 in voice 2: inserting the next continuing triplet's time on beat 2 would split it, so a
    // letter, a rest or a MIDI note there is refused, as a waiting tuplet is
    for (int entry = 0; entry < 3; ++entry) {
        MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();
        for (int pitch : { 60, 62, 64, 65 }) {
            enterNote(score, pitch);
        }
        is.setTrack(1);
        is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
        ASSERT_TRUE(waitForTuplet(score, true));
        for (int i = 0; i < 3; ++i) {
            enterNote(score, 55);
        }
        ASSERT_TRUE(score->sanityCheck());

        is.setTrack(0);
        is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
        is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
        score->select(toChord(is.cr())->upNote());
        is.setTrack(0);
        is.setDuration(DurationType::V_EIGHTH);
        is.resumeContinuingTuplet(ContinuingTuplet { { EIGHTH_TRIPLET } });
        EXPECT_FALSE(NoteInput::nextContinuingTupletFits(score, is));
        MScore::setError(MsError::MS_NO_ERROR);
        if (entry == 0) {
            insertLetter(score);
        } else if (entry == 1) {
            enterRest(score);
        } else {
            enterNote(score, 67);
        }
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::CANNOT_INSERT_TUPLET);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "4 4 4 4");
        EXPECT_EQ(rhythm(measureAt(score, 0), 1), "3:2[4 4 4] r2");
        MScore::setError(MsError::MS_NO_ERROR);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletInsertWhereOtherVoiceTupletStarts)
{
    // a quarter triplet on beats 2-3 in voice 2; the run's first triplet, inserted on beat 2, moves it to start at the
    // input position, where the next continuing triplet is inserted before it too
    MasterScore* score = pendingTupletScore(NoteEntryMethod::BY_NOTE_NAME);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    for (int pitch : { 60, 62, 64, 65 }) {
        enterNote(score, pitch);
    }
    is.setTrack(1);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    ASSERT_TRUE(waitForTuplet(score, true));
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 55);
    }
    ASSERT_TRUE(score->sanityCheck());

    is.setTrack(0);
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setTrack(0);
    is.setDuration(DurationType::V_EIGHTH);
    is.resumeContinuingTuplet(ContinuingTuplet { { EIGHTH_TRIPLET } });
    for (int i = 0; i < 3; ++i) {
        insertLetter(score);
    }
    EXPECT_TRUE(NoteInput::nextContinuingTupletFits(score, is));
    MScore::setError(MsError::MS_NO_ERROR);
    insertLetter(score);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "4 3:2[8 8 8] 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(rhythm(measureAt(score, 0), 1), "r4 r4 r4 3:2[4 4 4] r4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeClickElsewhere)
{
    // a click on E, which isn't straight after a triplet, inserts the note alone and ends the run
    MasterScore* score = continuingTupletInsertScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    Position pos;
    pos.segment = score->tick2segment(Fraction(3, 4), false, SegmentType::ChordRest);
    pos.staffIdx = 0;
    pos.line = 6;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        NoteInput::insertChordByInsertingTime(tx, score, pos);
    });
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 4 4 8 4");
    EXPECT_FALSE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeTie)
{
    // the tie key inserts the next triplet's time at the input position and ties the note before it into it
    MasterScore* score = continuingTupletInsertScore();
    ASSERT_TRUE(score);
    EditTie::cmdAddTie(score);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8~] 3:2[8 r8 r8] 4 4 4");
    const ChordRest* tied = chordRestAt(measureAt(score, 0), 3);
    ASSERT_TRUE(tied && tied->isChord());
    ASSERT_TRUE(toChord(tied)->upNote()->tieBack());
    EXPECT_EQ(toChord(tied)->upNote()->tieBack()->startNote(), toChord(chordRestAt(measureAt(score, 0), 2))->upNote());

    delete score;
}

// a second staff in the first part, as in a piano score
static void addLowerStaff(MasterScore* score)
{
    score->startCmd(TranslatableString::untranslatable("Continuing tuplet tests"));
    Staff* staff = Factory::createStaff(score->staff(0)->part());
    staff->setPart(score->staff(0)->part());
    score->undoInsertStaff(staff, 1);
    score->endCmd();
}

// like moving the last chord entered to the staff below
static void moveLastChordDown(Score* score, const Fraction& tick)
{
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
        score->undo(new ChangeChordStaffMove(toChord(continuingCrAt(score, tick)), 1));
    });
}

TEST_F(Engraving_TupletTests, continuingTupletTieCrossStaff)
{
    // the tie key after a cross-staff chord, where the next continuing triplet can't go: with no space left in the bar, it
    // ends continuing tuplet input and ties to an ordinary chord; Insert across another voice's tuplet is refused, and
    // nothing changes
    for (bool insert : { false, true }) {
        MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
        ASSERT_TRUE(score);
        addLowerStaff(score);
        InputState& is = score->inputState();
        if (insert) {
            // voice 2: a quarter triplet on beats 1-2; the run's first triplet on beat 1 in voice 1
            for (int pitch : { 60, 62, 64, 65 }) {
                is.setDuration(DurationType::V_QUARTER);
                enterNote(score, pitch);
            }
            is.setTrack(1);
            is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
            ASSERT_TRUE(waitForTuplet(score, true));
            for (int i = 0; i < 3; ++i) {
                enterNote(score, 55);
            }
            is.setTrack(0);
            is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
            score->select(toChord(is.cr())->upNote());
            is.setTrack(0);
            is.setDuration(DurationType::V_EIGHTH);
            is.resumeContinuingTuplet(ContinuingTuplet { { EIGHTH_TRIPLET } });
            for (int i = 0; i < 3; ++i) {
                enterNote(score);
            }
            is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
        } else {
            // 5 eighths and a continuing triplet up to 7/8, where the next triplet doesn't fit
            for (int i = 0; i < 5; ++i) {
                enterNote(score);
            }
            startContinuingTuplet(score, Fraction(5, 8));
            for (int i = 0; i < 3; ++i) {
                enterNote(score);
            }
        }
        const Fraction lastTick = insert ? Fraction(1, 6) : Fraction(19, 24);
        moveLastChordDown(score, lastTick);
        ASSERT_TRUE(continuingCrAt(score, lastTick));
        ASSERT_EQ(toChord(continuingCrAt(score, lastTick))->staffMove(), 1);
        EXPECT_FALSE(NoteInput::nextContinuingTupletFits(score, is));
        const std::string before = rhythm(measureAt(score, 0));

        MScore::setError(MsError::MS_NO_ERROR);
        EditTie::cmdAddTie(score);
        expectValid(score);
        if (insert) {
            EXPECT_NE(MScore::_error, MsError::MS_NO_ERROR);
            EXPECT_EQ(rhythm(measureAt(score, 0)), before);
            EXPECT_FALSE(toChord(continuingCrAt(score, lastTick))->upNote()->tieFor());
            EXPECT_TRUE(is.continuingTuplet());
        } else {
            EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
            EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8~] 8");
            EXPECT_TRUE(toChord(continuingCrAt(score, lastTick))->upNote()->tieFor());
            EXPECT_FALSE(is.continuingTuplet());
        }
        MScore::setError(MsError::MS_NO_ERROR);

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletInsertModeLinkedTabUndo)
{
    // a guitar staff with a linked TAB staff: the triplet's time is inserted on both, and one undo removes it
    MasterScore* score = ScoreRW::readScore(TUPLET_DATA_DIR + u"guitarLinkedTab.mscx");
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setTrack(0);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_QUARTER);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    for (int pitch : { 64, 65, 67, 69 }) {
        enterNote(score, pitch);
    }
    is.setNoteEntryMethod(NoteEntryMethod::TIMEWISE);
    is.setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    score->select(toChord(is.cr())->upNote());
    is.setDuration(DurationType::V_EIGHTH);
    is.resumeContinuingTuplet(ContinuingTuplet { { EIGHTH_TRIPLET } });
    insertLetter(score);
    expectValid(score);
    EXPECT_EQ(rhythm(score->firstMeasure()), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(rhythm(score->firstMeasure(), VOICES), "4 3:2[8 r8 r8] 4 4 4");

    score->undoRedo(true, nullptr);
    expectValid(score);
    EXPECT_EQ(rhythm(score->firstMeasure()), "4 4 4 4");
    EXPECT_EQ(rhythm(score->firstMeasure(), VOICES), "4 4 4 4");
    EXPECT_EQ(score->firstMeasure()->ticks(), Fraction(1, 1));
    EXPECT_TRUE(is.continuingTuplet());

    score->undoRedo(false, nullptr);
    expectValid(score);
    EXPECT_EQ(rhythm(score->firstMeasure()), "4 3:2[8 r8 r8] 4 4 4");
    EXPECT_EQ(rhythm(score->firstMeasure(), VOICES), "4 3:2[8 r8 r8] 4 4 4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletEndsElsewhere)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // a triplet, then the input position moves to the next bar
    for (int i = 0; i < 3; ++i) {
        enterNote(score);
    }
    score->inputState().setSegment(score->tick2segment(Fraction(1, 1), false, SegmentType::ChordRest));
    enterNote(score);
    expectValid(score);

    ChordRest* cr = continuingCrAt(score, Fraction(1, 1));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isChord());
    EXPECT_FALSE(cr->tuplet());
    EXPECT_FALSE(score->inputState().continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletGoesOnInTuplet)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // back to the start of the current triplet: the note is replaced and continuing input goes on
    enterNote(score, 60);
    enterNote(score, 62);
    score->inputState().setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    enterNote(score, 64);
    EXPECT_TRUE(score->inputState().continuingTuplet());

    ChordRest* first = continuingCrAt(score, Fraction(0, 1));
    ASSERT_TRUE(first && first->isChord());
    EXPECT_EQ(toChord(first)->upNote()->pitch(), 64);
    ASSERT_TRUE(first->tuplet());
    EXPECT_EQ(first->tuplet()->elements().size(), 3u);

    // straight after the triplet, the next one is created
    score->inputState().setSegment(score->tick2segment(Fraction(1, 4), false, SegmentType::ChordRest));
    enterNote(score, 65);
    expectValid(score);
    ChordRest* next = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(next);
    ASSERT_TRUE(next->tuplet());
    expectEighthTriplet(next->tuplet(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletResumed)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // a triplet, then Continue tuplets off for a quarter note on beat 2
    for (int i = 0; i < 3; ++i) {
        enterNote(score, 60 + i);
    }
    InputState& is = score->inputState();
    const ContinuingTuplet continuing = *is.continuingTuplet();
    is.setContinuingTuplet(std::nullopt);
    is.setDuration(DurationType::V_QUARTER);
    enterNote(score, 67);

    // on again on beat 3: the run goes on there, though no triplet ends there
    is.resumeContinuingTuplet(continuing);
    EXPECT_TRUE(is.continuingTupletGoesOn());
    is.setDuration(DurationType::V_EIGHTH);
    for (int i = 0; i < 4; ++i) {
        enterNote(score, 60 + i);
    }
    expectValid(score);
    EXPECT_TRUE(is.continuingTuplet());

    ChordRest* quarter = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(quarter);
    EXPECT_FALSE(quarter->tuplet());
    EXPECT_EQ(quarter->ticks(), Fraction(1, 4));
    for (const Fraction& tick : { Fraction(1, 2), Fraction(3, 4) }) {
        ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr);
        expectEighthTriplet(cr->tuplet(), tick);
    }

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletResumedEndsElsewhere)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // resumed on beat 1 of bar 2, then the input position moves on to bar 3
    InputState& is = score->inputState();
    const ContinuingTuplet continuing = *is.continuingTuplet();
    is.setContinuingTuplet(std::nullopt);
    is.setSegment(score->tick2segment(Fraction(1, 1), false, SegmentType::ChordRest));
    is.resumeContinuingTuplet(continuing);
    is.setSegment(score->tick2segment(Fraction(2, 1), false, SegmentType::ChordRest));
    EXPECT_FALSE(is.continuingTupletGoesOn());
    enterNote(score);
    expectValid(score);

    ChordRest* cr = continuingCrAt(score, Fraction(2, 1));
    ASSERT_TRUE(cr);
    EXPECT_TRUE(cr->isChord());
    EXPECT_FALSE(cr->tuplet());
    EXPECT_FALSE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletResumedNested)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // continuing eighth-note triplets inside a quarter-note triplet, stopped once it is full
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    startContinuingTuplet(score, Fraction(0, 1));
    for (int i = 0; i < 9; ++i) {
        enterNote(score, 60 + i);
    }
    InputState& is = score->inputState();
    const ContinuingTuplet continuing = *is.continuingTuplet();
    ASSERT_EQ(continuing.levels.size(), 2u);
    is.setContinuingTuplet(std::nullopt);

    // resumed in bar 2: the whole structure is created there
    is.setSegment(score->tick2segment(Fraction(1, 1), false, SegmentType::ChordRest));
    is.resumeContinuingTuplet(continuing);
    enterNote(score, 69);
    expectValid(score);
    EXPECT_TRUE(is.continuingTuplet());

    ChordRest* cr = continuingCrAt(score, Fraction(1, 1));
    ASSERT_TRUE(cr);
    expectEighthTriplet(cr->tuplet(), Fraction(1, 1));
    const Tuplet* outer = cr->tuplet()->tuplet();
    ASSERT_TRUE(outer);
    EXPECT_EQ(outer->tick(), Fraction(1, 1));
    EXPECT_EQ(outer->ratio(), Fraction(3, 2));
    EXPECT_EQ(outer->baseLen().type(), DurationType::V_QUARTER);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletPausedNested)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // continuing eighth-note triplets inside a quarter-note triplet
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    startContinuingTuplet(score, Fraction(0, 1));
    for (int i = 0; i < 9; ++i) {
        enterNote(score, 60 + i);
    }
    const ContinuingTuplet continuing = *score->inputState().continuingTuplet();
    ASSERT_EQ(continuing.levels.size(), 2u);
    const Tuplet* inner = continuingCrAt(score, Fraction(0, 1))->tuplet();
    ASSERT_TRUE(inner && inner->tuplet());
    const Tuplet* outer = inner->tuplet();

    // re-ticked straight after the outer tuplet, the paused run goes on with both levels
    const ContinuingTuplet continued = ContinuingTuplet::continuing(outer, continuing);
    ASSERT_EQ(continued.levels.size(), 2u);
    EXPECT_EQ(continued.levels.back().baseLen.type(), DurationType::V_EIGHTH);

    // with no paused run, or a different one, only the outer tuplet's own level
    EXPECT_EQ(ContinuingTuplet::continuing(outer, std::nullopt).levels.size(), 1u);
    ContinuingTuplet other = continuing;
    other.levels.front().ratio = Fraction(5, 4);
    EXPECT_EQ(ContinuingTuplet::continuing(outer, other).levels.size(), 1u);

    // the inner tuplet has both levels anyway
    EXPECT_EQ(ContinuingTuplet::continuing(inner, continuing).levels.size(), 2u);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsideTuplet)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // continuing eighth-note triplets inside a quarter-note triplet
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    const Tuplet* outer = continuingCrAt(score, Fraction(0, 1))->tuplet();
    ASSERT_TRUE(outer);
    startContinuingTuplet(score, Fraction(0, 1));

    // they continue until the quarter-note triplet is full
    for (int i = 0; i < 9; ++i) {
        enterNote(score, 60 + i);
    }
    expectValid(score);
    for (int i = 0; i < 3; ++i) {
        const Fraction tick = Fraction(i, 6);
        ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr);
        expectEighthTriplet(cr->tuplet(), tick);
        EXPECT_EQ(cr->tuplet()->tuplet(), outer);
    }

    // after it, the whole structure repeats: a new quarter-note triplet with an eighth-note triplet in it
    enterNote(score, 69);
    expectValid(score);
    EXPECT_TRUE(score->inputState().continuingTuplet());
    ChordRest* after = continuingCrAt(score, Fraction(1, 2));
    ASSERT_TRUE(after);
    expectEighthTriplet(after->tuplet(), Fraction(1, 2));
    const Tuplet* newOuter = after->tuplet()->tuplet();
    ASSERT_TRUE(newOuter);
    EXPECT_NE(newOuter, outer);
    EXPECT_EQ(newOuter->tick(), Fraction(1, 2));
    EXPECT_EQ(newOuter->ratio(), Fraction(3, 2));
    EXPECT_EQ(newOuter->baseLen().type(), DurationType::V_QUARTER);
    EXPECT_FALSE(newOuter->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsideTupletSpillOver)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // continuing eighth-note triplets inside a quarter-note triplet
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    startContinuingTuplet(score, Fraction(0, 1));
    for (int i = 0; i < 8; ++i) {
        enterNote(score, 60 + i);
    }

    // a quarter note on the last eighth of the structure continues, tied, in a new one
    score->inputState().setDuration(DurationType::V_QUARTER);
    enterNote(score, 68);
    expectValid(score);
    EXPECT_TRUE(score->inputState().continuingTuplet());

    ChordRest* next = continuingCrAt(score, Fraction(1, 2));
    ASSERT_TRUE(next);
    ASSERT_TRUE(next->isChord());
    EXPECT_TRUE(toChord(next)->upNote()->tieBack());
    expectEighthTriplet(next->tuplet(), Fraction(1, 2));
    const Tuplet* newOuter = next->tuplet()->tuplet();
    ASSERT_TRUE(newOuter);
    EXPECT_EQ(newOuter->tick(), Fraction(1, 2));
    EXPECT_EQ(newOuter->baseLen().type(), DurationType::V_QUARTER);

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsideTupletStopped)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // during continuing eighth-note triplets, a sixteenth-note triplet in the first eighth becomes the continuing tuplet
    const Tuplet* inner = nullptr;
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction&) {
        inner = score->addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_16TH),
                                 TupletNumberType::SHOW_NUMBER, TupletBracketType::AUTO_BRACKET);
    });
    ASSERT_TRUE(inner);
    const Tuplet* outer = inner->tuplet();
    ASSERT_TRUE(outer);
    is.setSegment(score->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_16TH);
    is.setContinuingTuplet(ContinuingTuplet::of(inner));

    // five continuing sixteenths, then continuing input stops (Escape) and sixteenths go on past the eighth-note triplet
    for (int i = 0; i < 5; ++i) {
        enterNote(score, 60 + i);
        expectValid(score);
    }
    is.setContinuingTuplet(std::nullopt);
    for (int i = 0; i < 9; ++i) {
        enterNote(score, 65 + i);
        expectValid(score);
    }

    // two sixteenth-note triplets in the first two eighths of the eighth-note triplet
    for (int i = 0; i < 6; ++i) {
        ChordRest* cr = continuingCrAt(score, Fraction(i, 36));
        ASSERT_TRUE(cr && cr->isChord() && cr->tuplet());
        EXPECT_EQ(cr->tuplet()->baseLen().type(), DurationType::V_16TH);
        EXPECT_EQ(cr->tuplet()->tuplet(), outer);
    }
    // ordinary sixteenths in its last eighth
    for (const Fraction& tick : { Fraction(4, 24), Fraction(5, 24) }) {
        ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr && cr->isChord());
        EXPECT_EQ(cr->durationType().type(), DurationType::V_16TH);
        EXPECT_EQ(cr->tuplet(), outer);
    }
    // ordinary sixteenths after it
    for (int i = 4; i < 8; ++i) {
        ChordRest* cr = continuingCrAt(score, Fraction(i, 16));
        ASSERT_TRUE(cr && cr->isChord());
        EXPECT_FALSE(cr->tuplet());
    }

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInsideTupletNoRoom)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // a quarter note, then a quarter-note triplet from beat 2 with continuing eighth-note triplets in it
    score->inputState().setDuration(DurationType::V_QUARTER);
    enterNote(score, 60);
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(1, 4)), Fraction(3, 2), TDuration(DurationType::V_QUARTER)));
    startContinuingTuplet(score, Fraction(1, 4));

    for (int i = 0; i < 9; ++i) {
        enterNote(score, 62 + i);
    }
    expectValid(score);
    EXPECT_TRUE(score->inputState().continuingTuplet());

    // another quarter-note triplet doesn't fit before the barline: the next note ends continuing tuplet input and goes in
    // as an ordinary one
    MScore::setError(MsError::MS_NO_ERROR);
    enterNote(score, 72);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_FALSE(score->inputState().continuingTuplet());
    ChordRest* after = continuingCrAt(score, Fraction(3, 4));
    ASSERT_TRUE(after);
    EXPECT_TRUE(after->isChord());
    EXPECT_FALSE(after->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletEndsOnVoiceChange)
{
    MasterScore* score = continuingTupletScore();
    ASSERT_TRUE(score);

    // in the middle of the triplet, input switches to voice 2: an ordinary note there, as without continuing input
    enterNote(score, 60);
    score->inputState().setTrack(1);
    enterNote(score, 67);
    expectValid(score);

    Segment* s = score->tick2segment(Fraction(1, 12), false, SegmentType::ChordRest);
    ASSERT_TRUE(s);
    ChordRest* voice2 = toChordRest(s->element(1));
    ASSERT_TRUE(voice2);
    EXPECT_TRUE(voice2->isChord());
    EXPECT_FALSE(voice2->tuplet());
    EXPECT_FALSE(score->inputState().continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletLinkedTab)
{
    MasterScore* score = continuingTupletScoreFrom(u"continuingTupletLinkedTab.mscx", 0);
    ASSERT_TRUE(score);

    // notes entered on the standard staff: the continuing triplets are also created on the linked tablature staff
    startContinuingTuplet(score, Fraction(0, 1));
    for (int i = 0; i < 4; ++i) {
        enterNote(score, 64);
    }
    expectValid(score);

    for (track_idx_t track : { track_idx_t(0), track_idx_t(VOICES) }) {
        ChordRest* cr = continuingCrAt(score, Fraction(1, 4), track);
        ASSERT_TRUE(cr);
        EXPECT_TRUE(cr->isChord());
        expectEighthTriplet(cr->tuplet(), Fraction(1, 4));
    }

    // undo removes the new triplet from both staves
    score->undoRedo(true, nullptr);
    expectValid(score);
    for (track_idx_t track : { track_idx_t(0), track_idx_t(VOICES) }) {
        ChordRest* cr = continuingCrAt(score, Fraction(1, 4), track);
        ASSERT_TRUE(cr);
        EXPECT_FALSE(cr->tuplet());
    }

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletTabFrets)
{
    MasterScore* score = continuingTupletScoreFrom(u"continuingTupletLinkedTab.mscx", VOICES);
    ASSERT_TRUE(score);

    // frets entered on the tablature staff (moving on after each, as with the right arrow) continue the triplets, on both staves
    score->inputState().setString(2);
    startContinuingTuplet(score, Fraction(0, 1), VOICES);
    for (int i = 0; i < 4; ++i) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            NoteInput::addFret(tx, score, 3);
        });
        score->inputState().moveToNextInputPos();
    }
    expectValid(score);

    ChordRest* tab = continuingCrAt(score, Fraction(1, 4), VOICES);
    ASSERT_TRUE(tab);
    ASSERT_TRUE(tab->isChord());
    EXPECT_EQ(toChord(tab)->upNote()->fret(), 3);
    expectEighthTriplet(tab->tuplet(), Fraction(1, 4));

    ChordRest* standard = continuingCrAt(score, Fraction(1, 4), 0);
    ASSERT_TRUE(standard);
    EXPECT_TRUE(standard->isChord());
    expectEighthTriplet(standard->tuplet(), Fraction(1, 4));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletDrums)
{
    MasterScore* score = continuingTupletScoreFrom(u"tupletDrums.mscx", 0);
    ASSERT_TRUE(score);

    auto enterDrum = [score](int drumPitch) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            NoteInput::addPitch(tx, score, NoteInputParams { 0, drumPitch }, false, false);
        });
    };

    // instruments in the same voice (snare, hi-hat) continue the triplets
    startContinuingTuplet(score, Fraction(0, 1));
    for (int i = 0; i < 3; ++i) {
        enterDrum(38);
        enterDrum(42);
    }
    expectValid(score);

    ChordRest* cr = continuingCrAt(score, Fraction(1, 4));
    ASSERT_TRUE(cr);
    ASSERT_TRUE(cr->isChord());
    EXPECT_EQ(toChord(cr)->upNote()->pitch(), 42);
    expectEighthTriplet(cr->tuplet(), Fraction(1, 4));
    EXPECT_TRUE(score->inputState().continuingTuplet());

    // an instrument in another voice (bass drum) ends continuing tuplet input, like a voice change
    enterDrum(36);
    expectValid(score);
    EXPECT_FALSE(score->inputState().continuingTuplet());

    ChordRest* bassDrum = continuingCrAt(score, Fraction(1, 2), 1);
    ASSERT_TRUE(bassDrum);
    EXPECT_TRUE(bassDrum->isChord());
    EXPECT_FALSE(bassDrum->tuplet());
    EXPECT_EQ(bassDrum->ticks(), Fraction(1, 8));

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletDrumsByDuration)
{
    MasterScore* score = continuingTupletScoreFrom(u"tupletDrums.mscx", 0);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();
    is.setNoteEntryMethod(NoteEntryMethod::BY_DURATION);

    // snares (voice 1) continue the triplets
    startContinuingTuplet(score, Fraction(0, 1));
    is.setNotes({ NoteVal(38) });
    for (int i = 0; i < 3; ++i) {
        pressDuration(score, DurationType::V_EIGHTH);
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] r4 r2");
    EXPECT_TRUE(is.continuingTuplet());

    // a bass drum (voice 2) ends continuing tuplet input, as a voice change does: no empty triplet in voice 1
    is.setNotes({ NoteVal(36) });
    pressDuration(score, DurationType::V_EIGHTH);
    expectValid(score);
    EXPECT_FALSE(is.continuingTuplet());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] r4 r2");
    ChordRest* bassDrum = continuingCrAt(score, Fraction(1, 4), 1);
    ASSERT_TRUE(bassDrum);
    EXPECT_TRUE(bassDrum->isChord());
    EXPECT_FALSE(bassDrum->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletDrumsNoRoom)
{
    MasterScore* score = continuingTupletScoreFrom(u"tupletDrums.mscx", 0);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // from a MIDI keyboard: the drum's voice is chosen when the note is added
    auto enterDrum = [score](int drumPitch) {
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            NoteInput::addMidiPitch(tx, score, drumPitch, false, false);
        });
    };

    // 5 snare eighths, a continuing triplet up to 7/8, then the next triplet doesn't fit
    for (int i = 0; i < 5; ++i) {
        enterDrum(38);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    for (int i = 0; i < 3; ++i) {
        enterDrum(38);
    }
    MScore::setError(MsError::MS_NO_ERROR);

    // a snare (voice 1) ends continuing tuplet input and goes in as an ordinary note
    enterDrum(38);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_FALSE(is.continuingTuplet());
    ChordRest* snare = continuingCrAt(score, Fraction(7, 8));
    ASSERT_TRUE(snare);
    EXPECT_TRUE(snare->isChord());
    EXPECT_FALSE(snare->tuplet());

    // after undo, a bass drum (voice 2) ends it too, as a voice change does
    score->undoRedo(true, nullptr);
    EXPECT_TRUE(is.continuingTuplet());
    enterDrum(36);
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_FALSE(is.continuingTuplet());
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] r8");
    ChordRest* bassDrum = continuingCrAt(score, Fraction(7, 8), 1);
    ASSERT_TRUE(bassDrum);
    EXPECT_TRUE(bassDrum->isChord());
    EXPECT_FALSE(bassDrum->tuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletRepeat)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // 5 eighths, then a continuing triplet up to 7/8 whose first note is a chord
    for (int i = 0; i < 5; ++i) {
        enterNote(score);
    }
    startContinuingTuplet(score, Fraction(5, 8));
    enterNote(score, 60);
    enterNote(score, 64, true);

    // R repeats the chord in the continuing triplet
    for (int i = 0; i < 2; ++i) {
        repeatChord(score, toChord(continuingCrAt(score, Fraction(5, 8))));
    }
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] r8");
    for (const Fraction& tick : { Fraction(5, 8), Fraction(17, 24), Fraction(19, 24) }) {
        const ChordRest* cr = continuingCrAt(score, tick);
        ASSERT_TRUE(cr && cr->isChord());
        EXPECT_EQ(toChord(cr)->notes().size(), 2u);
    }
    MScore::setError(MsError::MS_NO_ERROR);

    // where the next triplet doesn't fit, R ends continuing tuplet input and repeats the chord as an ordinary one
    repeatChord(score, toChord(continuingCrAt(score, Fraction(19, 24))));
    expectValid(score);
    EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 8 8] 8");
    EXPECT_EQ(toChord(continuingCrAt(score, Fraction(19, 24)))->notes().size(), 2u);
    const ChordRest* last = continuingCrAt(score, Fraction(7, 8));
    ASSERT_TRUE(last && last->isChord());
    EXPECT_EQ(toChord(last)->notes().size(), 2u);
    EXPECT_FALSE(is.continuingTuplet());

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletLocalTimeSig)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    // a local 2/4 in 4/4: two triplets fill the bar, and a half note spills from one into the next
    score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
        TimeSig* ts = Factory::createTimeSig(score->dummy());
        ts->setSig(Fraction(2, 4));
        EditTimeSig::addTimeSig(tx, score, score->firstMeasure(), 0, ts, true);
    });
    ASSERT_EQ(score->staff(0)->timeStretch(Fraction(0, 1)), Fraction(1, 2));

    startContinuingTuplet(score, Fraction(0, 1));
    enterNote(score, 60);
    score->inputState().setDuration(DurationType::V_HALF);
    enterNote(score, 62);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4~] 3:2[4 r8]");
    EXPECT_TRUE(score->inputState().continuingTuplet());

    // the next bar starts with a new triplet
    score->inputState().setDuration(DurationType::V_EIGHTH);
    enterNote(score, 64);
    enterNote(score, 64);
    expectValid(score);
    EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4~] 3:2[4 8]");
    EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[8 r8 r8] r4");

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // 13 eighths: four triplets fill the bar, the 13th starts a triplet in the next bar
        setPendingContinuingTriplet(score);
        for (int i = 0; i < 13; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8 8] 3:2[8 8 8] 3:2[8 8 8] 3:2[8 8 8]");
        EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[8 r8 r8] r4 r2");

        // a rest starts the next triplet too
        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_EIGHTH);
        is.setRest(true);
        pressDuration(score, DurationType::V_EIGHTH);
        is.setRest(false);
        pressDuration(score, DurationType::V_EIGHTH);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[8 8 8] 3:2[r8 8 r8] r2");
        EXPECT_TRUE(is.continuingTuplet());

        // undoing the rest takes its triplet away, and continuing tuplet input stays on
        score->undoRedo(true, nullptr);
        score->undoRedo(true, nullptr);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 1)), "3:2[8 8 8] r4 r2");
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletSpillOverByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // an eighth, then a half note: a quarter at the end of this triplet, tied to a quarter in the next one
        setPendingContinuingTriplet(score);
        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_HALF);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4~] 3:2[4 r8] r2");
        const Chord* tiedFrom = toChord(continuingCrAt(score, Fraction(1, 12)));
        const ChordRest* tiedTo = continuingCrAt(score, Fraction(1, 4));
        ASSERT_TRUE(tiedFrom && tiedTo && tiedTo->isChord());
        ASSERT_TRUE(tiedFrom->upNote()->tieFor());
        EXPECT_EQ(tiedFrom->upNote()->tieFor()->endNote(), toChord(tiedTo)->upNote());
        EXPECT_EQ(is.tick(), Fraction(5, 12));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletNoRoomSpillOverByDuration)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // 5 ordinary eighths, then a continuing triplet up to 7/8
        for (int i = 0; i < 5; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        setPendingContinuingTriplet(score);
        pressDuration(score, DurationType::V_EIGHTH);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 r8 r8] r8");
        MScore::setError(MsError::MS_NO_ERROR);

        // a half note would continue in a triplet that doesn't fit: it ends continuing tuplet input and goes in as
        // without it
        pressDuration(score, DurationType::V_HALF);
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 4~] 8~");
        EXPECT_FALSE(is.continuingTuplet());

        // after undo, a quarter fills the triplet
        score->undoRedo(true, nullptr);
        EXPECT_TRUE(is.continuingTuplet());
        pressDuration(score, DurationType::V_QUARTER);
        expectValid(score);
        EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[8 4] r8");
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletOtherVoiceSelected)
{
    // an eighth triplet on beat 2, then quarters in voice 2, as before a switch back to voice 1: continuing tuplet
    // input continues the triplet in voice 1, though the selection is in voice 2
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore();
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        is.setRest(true);
        pressDuration(score, DurationType::V_QUARTER);
        is.setRest(false);
        setPendingTriplet(score, true);
        for (int i = 0; i < 3; ++i) {
            pressDuration(score, DurationType::V_EIGHTH);
        }
        enterVoice2Quarters(score);
        ASSERT_EQ(rhythm(measureAt(score, 0)), "r4 3:2[8 8 8] r2");
        is.setNoteEntryMethod(method);
        is.setSegment(score->tick2segment(Fraction(1, 2), false, SegmentType::ChordRest));
        is.setDuration(DurationType::V_EIGHTH);
        is.setContinuingTuplet(ContinuingTuplet::of(continuingCrAt(score, Fraction(1, 4))->tuplet()));
        ASSERT_TRUE(score->selection().element());
        ASSERT_EQ(score->selection().element()->track(), 1);

        // the next triplet replaces part of the half rest
        enterDuration(score, DurationType::V_EIGHTH);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "r4 3:2[8 8 8] 3:2[8 r8 r8] r4");
        EXPECT_EQ(is.track(), 0);
        EXPECT_EQ(is.tick(), Fraction(1, 2) + Fraction(1, 12));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletOtherRatio)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);
    InputState& is = score->inputState();

    // an eighth triplet (3:2) on beat 1, and continuing eighth sextuplets (6:4): the triplet isn't one of them
    ASSERT_TRUE(addTuplet(continuingCrAt(score, Fraction(0, 1)), Fraction(3, 2), TDuration(DurationType::V_EIGHTH)));
    ContinuingTuplet sextuplet;
    sextuplet.levels.push_back({ Fraction(6, 4), TDuration(DurationType::V_EIGHTH), TupletNumberType::SHOW_NUMBER,
                                 TupletBracketType::AUTO_BRACKET });
    is.setContinuingTuplet(sextuplet);
    for (const Fraction& tick : { Fraction(0, 1), Fraction(1, 12), Fraction(1, 4) }) {
        is.setSegment(score->tick2segment(tick, false, SegmentType::ChordRest));
        EXPECT_FALSE(is.continuingTupletGoesOn());
    }

    delete score;
}

TEST_F(Engraving_TupletTests, continuingTupletInPart)
{
    MasterScore* score = continuingTupletScore(NoteEntryMethod::BY_NOTE_NAME, false);
    ASSERT_TRUE(score);

    score->startCmd(TranslatableString::untranslatable("Continuing tuplet tests"));
    Score* part = score->createScore();
    // the score owns and deletes the excerpt
    Excerpt* ex = new Excerpt(score);
    ex->setExcerptScore(part);
    ex->setName(u"Piano");
    ex->setParts({ score->parts().at(0) });
    Excerpt::createExcerpt(ex);
    score->undo(new AddExcerpt(ex));
    score->endCmd();

    // note input in the part: the continuing triplets are also created in the score
    InputState& is = part->inputState();
    is.setTrack(0);
    is.setSegment(part->tick2segment(Fraction(0, 1), false, SegmentType::ChordRest));
    is.setDuration(DurationType::V_EIGHTH);
    is.setNoteEntryMode(true);
    is.setNoteEntryMethod(NoteEntryMethod::BY_NOTE_NAME);
    startContinuingTuplet(part, Fraction(0, 1));
    for (int i = 0; i < 7; ++i) {
        enterNote(part, 60 + i);
    }
    expectValid(score);
    for (Score* s : { static_cast<Score*>(score), part }) {
        EXPECT_EQ(rhythm(s->firstMeasure()), "3:2[8 8 8] 3:2[8 8 8] 3:2[8 r8 r8] r4");
    }

    // undo removes the new triplet from both
    score->undoRedo(true, nullptr);
    expectValid(score);
    for (Score* s : { static_cast<Score*>(score), part }) {
        EXPECT_EQ(rhythm(s->firstMeasure()), "3:2[8 8 8] 3:2[8 8 8] r2");
    }

    delete score;
}

//-----------------------------------------
//    continuing tuplets with dotted durations
//     single and double dots in continuing eighth note
//     triplets, in each input method
//-----------------------------------------

TEST_F(Engraving_TupletTests, continuingTupletDotted)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // two dotted eighths fill each triplet
        startContinuingTuplet(score, Fraction(0, 1));
        for (int i = 0; i < 4; ++i) {
            enterDuration(score, DurationType::V_EIGHTH, 1);
        }
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8. 8.] 3:2[8. 8.] r2");
        EXPECT_EQ(is.tick(), Fraction(1, 2));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDottedSpillOver)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // an eighth, then a dotted quarter: a quarter at the end of this triplet, tied to an eighth in the next one
        startContinuingTuplet(score, Fraction(0, 1));
        enterDuration(score, DurationType::V_EIGHTH);
        enterDuration(score, DurationType::V_QUARTER, 1);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 4~] 3:2[8 r8 r8] r2");
        const Chord* tiedFrom = toChord(continuingCrAt(score, Fraction(1, 12)));
        const ChordRest* tiedTo = continuingCrAt(score, Fraction(1, 4));
        ASSERT_TRUE(tiedFrom && tiedTo && tiedTo->isChord());
        ASSERT_TRUE(tiedFrom->upNote()->tieFor());
        EXPECT_EQ(tiedFrom->upNote()->tieFor()->endNote(), toChord(tiedTo)->upNote());
        EXPECT_EQ(is.tick(), Fraction(1, 3));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDoubleDotted)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // two double-dotted eighths: the second is an eighth tied to a 32nd at the end of this triplet,
        // tied to a sixteenth in the next one
        startContinuingTuplet(score, Fraction(0, 1));
        enterDuration(score, DurationType::V_EIGHTH, 2);
        enterDuration(score, DurationType::V_EIGHTH, 2);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8.. 8~ 32~] 3:2[16 r16 r8 r8] r2");
        EXPECT_EQ(is.tick(), Fraction(7, 24));
        const ChordRest* last = continuingCrAt(score, Fraction(1, 4));
        ASSERT_TRUE(last && last->isChord());
        EXPECT_TRUE(toChord(last)->upNote()->tieBack());
        expectEighthTriplet(last->tuplet(), Fraction(1, 4));
        EXPECT_TRUE(is.continuingTuplet());

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDottedRest)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            // a dotted rest, a dotted note, and a dotted rest that starts the next triplet
            startContinuingTuplet(score, Fraction(0, 1));
            enterDuration(score, DurationType::V_EIGHTH, dots, true);
            enterDuration(score, DurationType::V_EIGHTH, dots);
            enterDuration(score, DurationType::V_EIGHTH, dots, true);
            expectValid(score);
            EXPECT_EQ(rhythm(measureAt(score, 0)),
                      dots == 1 ? "3:2[r8. 8.] 3:2[r8. r16 r8] r2" : "3:2[r8.. 8~ 32~] 3:2[16 r8.. r32 r16] r2");
            EXPECT_TRUE(is.continuingTuplet());

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDottedNoteValue)
{
    // the dots chosen before the triplet command: dotted or double-dotted eighth triplets
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        for (int dots : { 1, 2 }) {
            MasterScore* score = pendingTupletScore(method);
            ASSERT_TRUE(score);
            InputState& is = score->inputState();

            is.setDots(dots);
            setPendingContinuingTriplet(score);
            for (int i = 0; i < 4; ++i) {
                pressDuration(score, DurationType::V_EIGHTH);
            }
            expectValid(score);
            EXPECT_EQ(rhythm(measureAt(score, 0)),
                      dots == 1 ? "3:2[8. 8. 8.] 3:2[8. r8. r8.] r4" : "3:2[8.. 8.. 8..] 3:2[8.. r8.. r8..] r8");
            const Fraction second = dots == 1 ? Fraction(3, 8) : Fraction(7, 16);
            const ChordRest* cr = continuingCrAt(score, second);
            ASSERT_TRUE(cr && cr->tuplet());
            EXPECT_EQ(cr->tuplet()->tick(), second);
            EXPECT_EQ(cr->tuplet()->baseLen().type(), DurationType::V_EIGHTH);
            EXPECT_EQ(cr->tuplet()->baseLen().dots(), dots);
            EXPECT_TRUE(is.continuingTuplet());

            delete score;
        }
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDottedNoRoom)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_NOTE_NAME, NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // 5 ordinary eighths, then a dotted quarter fills a continuing triplet up to 7/8
        for (int i = 0; i < 5; ++i) {
            enterDuration(score, DurationType::V_EIGHTH);
        }
        startContinuingTuplet(score, Fraction(5, 8));
        enterDuration(score, DurationType::V_QUARTER, 1);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[4.] r8");
        MScore::setError(MsError::MS_NO_ERROR);

        // the next triplet doesn't fit: a dotted or double-dotted note or rest ends continuing tuplet input and goes in
        // as an ordinary one; undoing it brings continuing tuplet input back
        for (int dots : { 1, 2 }) {
            for (bool rest : { false, true }) {
                enterDuration(score, DurationType::V_16TH, dots, rest);
                expectValid(score);
                EXPECT_EQ(MScore::_error, MsError::MS_NO_ERROR);
                const std::string r = rest ? "r" : "";
                EXPECT_EQ(rhythm(measureAt(score, 0)),
                          "8 8 8 8 8 3:2[4.] " + r + (dots == 1 ? "16. r32" : "16.. r64"));
                EXPECT_FALSE(is.continuingTuplet());
                score->undoRedo(true, nullptr);
                EXPECT_EQ(rhythm(measureAt(score, 0)), "8 8 8 8 8 3:2[4.] r8");
                EXPECT_TRUE(is.continuingTuplet());
            }
        }

        delete score;
    }
}

TEST_F(Engraving_TupletTests, continuingTupletDotPreviousNote)
{
    for (NoteEntryMethod method : { NoteEntryMethod::BY_DURATION, NoteEntryMethod::RHYTHM }) {
        MasterScore* score = pendingTupletScore(method);
        ASSERT_TRUE(score);
        InputState& is = score->inputState();

        // two eighths, then the second one dotted: the input position moves on, and continuing input goes on
        setPendingContinuingTriplet(score);
        pressDuration(score, DurationType::V_EIGHTH);
        pressDuration(score, DurationType::V_EIGHTH);
        score->transactionManager()->transaction(TranslatableString::untranslatable("Continuing tuplet tests"), [&](Transaction& tx) {
            NoteInput::toggleDots(tx, score, 1, true /*toggleForSelectionOnly*/);
        });
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8. r16] r4 r2");
        EXPECT_EQ(is.tick(), Fraction(5, 24));
        EXPECT_TRUE(is.continuingTuplet());

        // a sixteenth fills the triplet, and the next eighth starts the next one
        pressDuration(score, DurationType::V_16TH);
        pressDuration(score, DurationType::V_EIGHTH);
        expectValid(score);
        EXPECT_EQ(rhythm(measureAt(score, 0)), "3:2[8 8. 16] 3:2[8 r8 r8] r2");

        delete score;
    }
}
