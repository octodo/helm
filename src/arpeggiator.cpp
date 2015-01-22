/* Copyright 2013-2015 Matt Tytel
 *
 * mopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mopo.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "arpeggiator.h"

#include "utils.h"
#include "voice_handler.h"

namespace mopo {

  namespace {
    const float MIN_VOICE_TIME = 0.01;
  } // namespace

  Arpeggiator::Arpeggiator(VoiceHandler* voice_handler) :
      Processor(kNumInputs, 1), voice_handler_(voice_handler),
      sustain_(false), phase_(1.0), note_index_(-1), last_played_note_(0) {
    MOPO_ASSERT(voice_handler);
  }

  void Arpeggiator::process() {
    if (active_notes_.size() == 0) {
      if (last_played_note_ >= 0) {
        voice_handler_->noteOff(last_played_note_, 0);
        last_played_note_ = -1;
      }
      return;
    }

    mopo_float frequency = input(kFrequency)->at(0);
    float min_gate = (MIN_VOICE_TIME + VOICE_KILL_TIME) * frequency;
    mopo_float gate = INTERPOLATE(min_gate, 1.0, input(kGate)->at(0));

    mopo_float delta_phase = frequency / sample_rate_;
    mopo_float new_phase = phase_ + buffer_size_ * delta_phase;

    if (new_phase >= gate && last_played_note_ >= 0) {
      int offset = CLAMP((gate - phase_) / delta_phase, 0, buffer_size_ - 1);
      voice_handler_->noteOff(last_played_note_, offset);
      last_played_note_ = -1;
    }
    if (new_phase >= 1) {
      int offset = CLAMP((1 - phase_) / delta_phase, 0, buffer_size_ - 1);
      mopo_float note = getNextNote();
      voice_handler_->noteOn(note, active_notes_[note], offset);
      last_played_note_ = note;
      phase_ = new_phase - 1.0;
    }
    else
      phase_ = new_phase;
  }

  mopo_float Arpeggiator::getNextNote() {
    note_index_ = (note_index_ + 1) % as_played_.size();
    return as_played_[note_index_];
  }

  void Arpeggiator::addNoteToPatterns(mopo_float note) {
    as_played_.push_back(note);
  }

  void Arpeggiator::removeNoteFromPatterns(mopo_float note) {
    as_played_.erase(
        std::remove(as_played_.begin(), as_played_.end(), note));
    if (as_played_.size() == 0)
      note_index_ = -1;
    else
      note_index_ %= as_played_.size();
  }

  void Arpeggiator::sustainOn() {
    sustain_ = true;
  }

  void Arpeggiator::sustainOff() {
    sustain_ = false;
    std::set<mopo_float>::iterator iter = sustained_notes_.begin();
    for (; iter != sustained_notes_.end(); ++iter)
      noteOff(*iter);
    sustained_notes_.clear();
  }

  void Arpeggiator::noteOn(mopo_float note, mopo_float velocity, int sample) {
    if (active_notes_.count(note))
      return;
    if (pressed_notes_.size() == 0) {
      note_index_ = -1;
      phase_ = 1.0;
    }
    active_notes_[note] = velocity;
    pressed_notes_.insert(note);
    addNoteToPatterns(note);
  }

  void Arpeggiator::noteOff(mopo_float note, int sample) {
    if (pressed_notes_.count(note) == 0)
      return;

    if (sustain_)
      sustained_notes_.insert(note);
    else {
      active_notes_.erase(note);
      removeNoteFromPatterns(note);
    }

    pressed_notes_.erase(note);
  }
} // namespace mopo
