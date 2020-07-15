// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Chain state.

#include "stages/chain_state.h"

#include <algorithm>

#include "stages/drivers/serial_link.h"
#include "stages/settings.h"

namespace stages {
  
using namespace std;

const uint32_t kLeftKey = stmlib::FourCC<'d', 'i', 's', 'c'>::value;
const uint32_t kRightKey = stmlib::FourCC<'o', 'v', 'e', 'r'>::value;

// How long before unpatching an input actually breaks the chain.
const uint32_t kUnpatchedInputDelay = 2000;
const int32_t kLongPressDuration = 800;

void ChainState::Init(SerialLink* left, SerialLink* right) {
  index_ = 0;
  size_ = 1;
  
  left_ = left;
  right_ = right;
  
  STATIC_ASSERT(sizeof(Packet) == kPacketSize, BAD_PACKET_SIZE);
  
  left_->Init(
      SERIAL_LINK_DIRECTION_LEFT,
      115200 * 8,
      left_rx_packet_[0].bytes,
      kPacketSize);
  right_->Init(
      SERIAL_LINK_DIRECTION_RIGHT,
      115200 * 8,
      right_rx_packet_[0].bytes,
      kPacketSize);
  
  ChannelState c = { .flags = 0xf0, .pot = 128, .cv_slider = 32768 };

  fill(&channel_state_[0], &channel_state_[kMaxNumChannels], c);
  fill(&unpatch_counter_[0], &unpatch_counter_[kNumChannels], 0);
  fill(&loop_status_[0], &loop_status_[kNumChannels], LOOP_STATUS_NONE);
  fill(&harmosc_status_[0], &harmosc_status_[kNumChannels], HARMOSC_STATUS_NONE);
  fill(&switch_pressed_[0], &switch_pressed_[kMaxChainSize], 0);
  fill(&switch_press_time_[0], &switch_press_time_[kMaxNumChannels], 0);
  
  request_.request = REQUEST_NONE;
  
  discovering_neighbors_ = true;
  ouroboros_ = false;
  counter_ = 0;
  num_internal_bindings_ = 0;
  num_bindings_ = 0;
}

void ChainState::DiscoverNeighbors() {
  // Between t = 500ms and t = 1500ms, ping the neighbors every 50ms
  if (counter_ >= 2000 &&
      counter_ <= 6000 &&
      (counter_ % 200) == 0) {
    left_tx_packet_.discovery.key = kLeftKey;
    left_tx_packet_.discovery.counter = size_;
    left_->Transmit(left_tx_packet_);
    
    right_tx_packet_.discovery.key = kRightKey;
    right_tx_packet_.discovery.counter = index_;
    right_->Transmit(right_tx_packet_);
  }
  
  const DiscoveryPacket* l = left_->available_rx_buffer<DiscoveryPacket>();
  if (l && l->key == kRightKey) {
    index_ = size_t(l->counter) + 1;
    size_ = std::max(size_, index_ + 1);
  }
  
  const DiscoveryPacket* r = right_->available_rx_buffer<DiscoveryPacket>();
  if (r && r->key == kLeftKey) {
    size_ = std::max(size_, size_t(r->counter));
  }
  
  ouroboros_ = index_ >= kMaxChainSize || size_ > kMaxChainSize;

  // The discovery phase lasts 2000ms.
  discovering_neighbors_ = counter_ < 8000 && !ouroboros_;
  if (discovering_neighbors_) {
    ++counter_;
  } else {
    counter_ = 0;
  }
}

void ChainState::TransmitRight() {
  if (index_ == size_ - 1) {
    return;
  }
  
  LeftToRightPacket* p = &right_tx_packet_.to_right;
  p->phase = tx_last_sample_.phase;
  p->segment = tx_last_sample_.segment;
  p->last_patched_channel = tx_last_patched_channel_;
  p->last_loop = tx_last_loop_;
  p->harmosc_fundamental = tx_harmosc_fundamental_;
  
  copy(&input_patched_[0], &input_patched_[index_ + 1], &p->input_patched[0]);
  copy(&switch_pressed_[0], &switch_pressed_[index_ + 1],
       &p->switch_pressed[0]);
  right_->Transmit(right_tx_packet_);
}

void ChainState::ReceiveRight() {
  if (index_ == size_ - 1) {
    return;
  }
  
  const RightToLeftPacket* p = right_->available_rx_buffer<RightToLeftPacket>();
  if (p) {
    size_t rx_index = p->channel[0].index();
    if (rx_index > index_ && rx_index < size_) {
      // This packet contains the state of a module on the right.
      // Check if some settings have been changed on the remote modules,
      // then update our local copy of its state.
      for (size_t i = 0; i < kNumChannels; ++i) {
        dirty_[remote_channel_index(rx_index, i)] = \
            remote_channel(rx_index, i)->flags != p->channel[i].flags;
      }
      copy(
          &p->channel[0],
          &p->channel[kNumChannels],
          remote_channel(rx_index, 0));
      request_.request = REQUEST_NONE;
//    } else if (rx_index == 0xf) {
    } else if (rx_index == 0x7) {
      // NOTE: I'm pretty sure the above change to the if-statement correctly
      // solves for the updates to how index extraction works, but not 100%
      // positive.

      // This suspiciously looks like a state change request packet!
      // We will take care of it later.
      request_ = *(const RequestPacket*)(p);
    }
  }
}

void ChainState::TransmitLeft() {
  if (index_ == 0) {
    return;
  }
  
  if (request_.request != REQUEST_NONE) {
    // Forward the request to the left.
    left_tx_packet_.request = request_;
  } else {
    // Determine which module contains the last segment of the chain
    // starting at this module's last segment.
    //
    // For example:
    //
    // 0----- 1----- 2----- 3----- 4-----
    // 
    // ----X- ------ ---X-- ------ ---X--
    //                            
    // last = 2
    //        last = 2
    //               last = 4
    //                      last = 4
    //                             last = 5
    size_t last = size_ - 1;
    for (size_t i = index_; i < size_; ++i) {
      for (size_t j = 0; j < kNumChannels; ++j) {
        if (remote_channel(i, j)->input_patched()) {
          last = i;
          goto found;
        }
      }
    }
  found:
    // In the example above, module 1 will alternate between sending to module
    // 0 its own state, and the state of module 2.
    size_t tx_index = index_ + ((counter_ >> 2) % (last - index_ + 1));
    copy(
        remote_channel(tx_index, 0),
        remote_channel(tx_index, kNumChannels),
        &left_tx_packet_.to_left.channel[0]);
  }
  left_->Transmit(left_tx_packet_);
}

void ChainState::ReceiveLeft() {
  if (index_ == 0) {
    rx_last_patched_channel_ = size_ * kNumChannels;
    rx_last_loop_.start = -1;
    rx_last_loop_.end = -1;
    rx_harmosc_fundamental_ = -1.0f;
    return;
  }
  
  const LeftToRightPacket* p = left_->available_rx_buffer<LeftToRightPacket>();
  if (p) {
    rx_last_patched_channel_ = p->last_patched_channel;
    rx_last_loop_ = p->last_loop;
    rx_last_sample_.phase = p->phase;
    rx_last_sample_.segment = p->segment;
    rx_harmosc_fundamental_ = p->harmosc_fundamental;
    copy(&p->switch_pressed[0], &p->switch_pressed[index_],
         &switch_pressed_[0]);
    copy(&p->input_patched[0], &p->input_patched[index_], &input_patched_[0]);
  }
}

void ChainState::Configure(
    SegmentGenerator* segment_generator,
    Oscillator* oscillator) {
  size_t last_local_channel = local_channel_index(0) + kNumChannels;
  size_t last_channel = size_ * kNumChannels;
  size_t last_patched_channel = rx_last_patched_channel_;
  Loop last_loop = rx_last_loop_;
  float harmosc_fundamental = rx_harmosc_fundamental_;

  // TODO - having configured the oscillator in the right cases, how are we
  // going to check when to use it in the stages.cc Process method?

  num_internal_bindings_ = 0;
  num_bindings_ = 0;
  num_local_harmosc_bindings_ = 0;
  num_remote_harmosc_bindings_ = 0;
  
  segment::Configuration configuration[kMaxNumChannels];
  uint8_t harmosc_waveshapes[kMaxNumChannels];
  
  for (size_t i = 0; i < kNumChannels; ++i) {
    size_t channel = local_channel_index(i);
    
    if (!local_channel(i)->input_patched()) {
      if (channel > last_patched_channel) {
        // Create a slave channel - we are just extending a chain of segments.
        size_t segment = channel - last_patched_channel;
        segment_generator[i].ConfigureSlave(segment);
        harmosc_status_[i] = HARMOSC_STATUS_NONE;
        set_loop_status(i, segment, last_loop);
      } else {
        segment::Configuration c = local_channel(i)->configuration();

        if (harmosc_fundamental < 0.0 && !c.harmosc) {
          // Create a free-running channel.
          segment_generator[i].ConfigureSingleSegment(false, c);
          binding_[num_bindings_].generator = i;
          binding_[num_bindings_].source = i;
          binding_[num_bindings_].destination = 0;
          ++num_bindings_;
          ++num_internal_bindings_;
          loop_status_[i] = c.loop ? LOOP_STATUS_SELF : LOOP_STATUS_NONE;
          harmosc_status_[i] = HARMOSC_STATUS_NONE;
        } else if (harmosc_fundamental < 0.0 && c.harmosc) {
          // Create a harmosc block, trying to extend it as far as possible.
          size_t num_channels = 0;
          bool add_more_segments = true;
          bool dirty = false;

          while (add_more_segments) {
            // Add an entry in the waveshape array.
            uint8_t waveshape = channel_state_[channel].flags & 0x7;
            harmosc_waveshapes[num_channels] = waveshape;

            dirty |= dirty_[channel];
            harmosc_fundamental = oscillator[i].fundamental();

            // Add a binding in the harmosc binding array.
            if (channel < last_local_channel) {
              // Bind local CV/pot to this segment's parameters.
              harmosc_local_binding_[num_local_harmosc_bindings_].generator = i;
              harmosc_local_binding_[num_local_harmosc_bindings_].destination =
                  num_channels == 0 ? -1 : num_channels;
              harmosc_local_binding_[num_local_harmosc_bindings_].source = i + num_channels;
              ++num_local_harmosc_bindings_;
            } else {
              // Bind remote CV/pot to this segment's parameters.
              harmosc_remote_binding_[num_remote_harmosc_bindings_].generator = i;
              harmosc_remote_binding_[num_remote_harmosc_bindings_].destination = num_channels;
              harmosc_remote_binding_[num_remote_harmosc_bindings_].source = channel;
              ++num_remote_harmosc_bindings_;
            }
            add_more_segments = num_channels == 0 ||
                !channel_state_[channel].configuration().harmosc;
            ++channel;
            ++num_channels;
            add_more_segments &= channel < last_channel
          }
          if (dirty || num_channels != oscillator[i].num_channels()) {
            oscillator[i].Configure(num_channels, harmosc_waveshapes);
          }
          loop_status_[i] = LOOP_STATUS_NONE;
          harmosc_status_[i] = HARMOSC_STATUS_START;
        } else {
          harmosc_local_binding_[num_local_harmosc_bindings_].generator = i;
          harmosc_local_binding_[num_local_harmosc_bindings_].source = i;
          harmosc_local_binding_[num_local_harmosc_bindings_].destination = 0;
          ++num_local_harmosc_bindings_;
          uint8_t waveshape = channel_state_[channel].flags & 0x7;

          oscillator[i].ConfigureSlave(harmosc_fundamental, waveshape);
          loop_status_[i] = LOOP_STATUS_NONE;
          harmosc_status_[i] = c.harmosc ? HARMOSC_STATUS_END : HARMOSC_STATUS_MIDDLE;
          if (c.harmosc) harmosc_fundamental = -1.0f;
        }
      }
    } else {
      last_patched_channel = channel;
      harmosc_fundamental = -1.0f;
      
      // Create a normal channel, trying to extend it as far as possible.
      size_t num_segments = 0;
      bool add_more_segments = true;
      bool dirty = false;
      
      last_loop.start = -1;
      last_loop.end = -1;
      while (add_more_segments) {
        // Add an entry in the configuration array.
        segment::Configuration c = channel_state_[channel].configuration();
        configuration[num_segments] = c;
        dirty |= dirty_[channel];
        
        if (c.loop) {
          if (last_loop.start == -1) last_loop.start = num_segments;
          last_loop.end = num_segments;
        }
        
        // Add a binding in the binding array.
        binding_[num_bindings_].generator = i;
        binding_[num_bindings_].destination = num_segments;
        if (channel < last_local_channel) {
          // Bind local CV/pot to this segment's parameters.
          binding_[num_bindings_].source = i + num_segments;
          ++num_internal_bindings_;
        } else {
          // Bind remote CV/pot to this segment's parameters.
          binding_[num_bindings_].source = channel;
        }
        ++num_bindings_;
        ++channel;
        ++num_segments;
        
        add_more_segments = channel < last_channel && \
             !channel_state_[channel].input_patched();
      }
      if (dirty || num_segments != segment_generator[i].num_segments()) {
        segment_generator[i].Configure(true, configuration, num_segments);
      }
      harmosc_status_[i] = HARMOSC_STATUS_NONE;
      set_loop_status(i, 0, last_loop);
    }
  }

  tx_last_loop_ = last_loop;
  tx_last_patched_channel_ = last_patched_channel;
  tx_harmosc_fundamental_ = harmosc_fundamental;
}

inline void ChainState::UpdateLocalState(
    const IOBuffer::Block& block,
    const Settings& settings,
    const SegmentGenerator::Output& last_out,
    Oscillator* oscillator) {
  tx_last_sample_ = last_out;
  
  ChannelBitmask input_patched_bitmask = 0;
  for (size_t i = 0; i < kNumChannels; ++i) {
    if (block.input_patched[i]) {
      unpatch_counter_[i] = 0;
    } else if (unpatch_counter_[i] < kUnpatchedInputDelay) {
      ++unpatch_counter_[i];
    }
    
    bool input_patched = unpatch_counter_[i] < kUnpatchedInputDelay;
    dirty_[local_channel_index(i)] = local_channel(i)->UpdateFlags(
        index_,
        settings.state().segment_configuration[i],
        input_patched);
    if (input_patched) {
      input_patched_bitmask |= 1 << i;
    }
  }
  input_patched_[index_] = input_patched_bitmask;

  for (size_t i = 0; i < kNumChannels; ++i) {
    switch (harmosc_status_[i]) {
      case HARMOSC_STATUS_START:
        tx_harmosc_fundamental_ = oscillator[i].fundamental();
        break;
      case HARMOSC_STATUS_END:
        tx_harmosc_fundamental_ = -1.0f;
        break;
    }
  }

  // NOTE: If there are issues with the current harmosc cancellation logic for input
  // patching, this could be another place where we could try to deal with it.
}

inline void ChainState::UpdateLocalPotCvSlider(const IOBuffer::Block& block) {
  for (size_t i = 0; i < kNumChannels; ++i) {
    ChannelState* s = local_channel(i);
    s->pot = block.pot[i] * 256.0f;
    s->cv_slider = block.cv_slider[i] * 16384.0f + 32768.0f;
  }
}

inline void ChainState::BindRemoteParameters(
    SegmentGenerator* segment_generator,
    Oscillator* oscillator) {
  for (size_t i = num_internal_bindings_; i < num_bindings_; ++i) {
    const ParameterBinding& m = binding_[i];
    segment_generator[m.generator].set_segment_parameters(
        m.destination,
        channel_state_[m.source].cv_slider / 16384.0f - 2.0f,
        channel_state_[m.source].pot / 256.0f);
  }

  for (size_t i = 0; i < num_remote_harmosc_bindings_; ++i) {
    const ParameterBinding& m = harmosc_remote_binding_[i];
    oscillator[m.generator].set_amplitude_and_harmonic_ratio(
        m.destination,
        channel_state_[m.source].cv_slider / 16384.0f - 2.0f,
        channel_state_[m.source].pot / 256.0f);
  }
}

inline void ChainState::BindLocalParameters(
    const IOBuffer::Block& block,
    SegmentGenerator* segment_generator,
    Oscillator* oscillator) {
  for (size_t i = 0; i < num_internal_bindings_; ++i) {
    const ParameterBinding& m = binding_[i];
    segment_generator[m.generator].set_segment_parameters(
        m.destination,
        block.cv_slider[m.source],
        block.pot[m.source]);
  }

  // TODO: frequency locking
  for (size_t i = 0; i < num_local_harmosc_bindings_; ++i) {
    const ParameterBinding& m = harmosc_local_binding_[i];
    if (m.destination == -1) {
      oscillator[m.generator].set_fundamental(
          block.cv_slider[m.source],
          block.pot[m.source]);
    } else {
      oscillator[m.generator].set_amplitude_and_harmonic_ratio(
          m.destination,
          block.cv_slider[m.source],
          block.pot[m.source]);
    }
  }
}

ChainState::RequestPacket ChainState::MakeLoopChangeRequest(
    size_t loop_start, size_t loop_end) {
  size_t channel_index = 0;
  size_t group_start = 0;
  size_t group_end = size_ * kNumChannels;
  size_t harmosc_segments_before_loop_start = 0;
  size_t harmosc_segments_before_loop_end = 0;

  RequestPacket result;

  // Fill group_start and group_end, which contain the tightest interval
  // of patched channels enclosing the loop.
  //
  // LOOP     ----S- ------ --E--- ------
  // PATCHED  -x---- ------ ----x- ------
  //           ^  ^           ^ ^        
  //           |  |           | |        
  //           |  |           | group_end
  //           |  |           loop_end
  //           |  loop_start
  //           group_start
  for (size_t i = 0; i < size_; ++i) {
    ChannelBitmask input_patched = input_patched_[i];
    for (size_t j = 0; j < kNumChannels; ++j) {
      if (input_patched & 1) {
        if (channel_index <= loop_start) {
          group_start = channel_index;
        } else if (channel_index >= loop_end) {
          group_end = min(group_end, channel_index);
        }
        // There shouldn't be a patched channel between the loop start
        // and the loop end.
        if (channel_index > loop_start && channel_index < loop_end) {
          // LOOP     ----S- ------ --E--- ------
          // PATCHED  -x---- ---x-- ----x- ------
          result.request = REQUEST_NONE;  // inconsistent loop
          return result;
        }
      }
      if (channel_index < loop_start && harmosc_start_or_end_[channel_index]) {
        ++harmosc_segments_before_loop_start;
      }
      if (channel_index < loop_end && harmosc_start_or_end_[channel_index]) {
        ++harmosc_segments_before_loop_end;
      }
      input_patched >>= 1;
      ++channel_index;
    }
  }
  
  // A "loop" spanning multiple channels among the first group of
  // unpatched channels is treated as a new harmonic oscillator range.
  if (group_start == 0 && !(input_patched_[0] & 1)) {
    if (loop_start != loop_end) {
      bool inconsistent_harmosc_range = false;
      // LOOP     -S-E-- ------
      // PATCHED  -----x ---x--
      if (group_end == loop_end) {
        // Harmosc ranges shouldn't include any patched inputs.
        inconsistent_harmosc_range = true;
      } else if (harmosc_segments_before_loop_start % 2 == 1) {
        // Since harmosc ranges have a minimum length of two, they always come in
        // pairs. Therefore there should never be an odd number of harmosc range
        // start/end points before a new one begins.
        inconsistent_harmosc_range = true;
      } else if (harmosc_segments_before_loop_end - harmosc_segments_before_loop_start > 1) {
        inconsistent_harmosc_range = true;
      } else if (harmosc_segments_before_loop_end - harmosc_segments_before_loop_start == 1) {
        // The only time there should be any segments between loop start and end is if
        // this is a request to cancel a harmosc range.
        if (!harmosc_start_or_end_[loop_start] || !harmosc_start_or_end_[loop_end]) {
          inconsistent_harmosc_range = true;
        }
      } else if (harmosc_start_or_end_[loop_end]) {
        inconsistent_harmosc_range = true;
      }

      if (inconsistent_harmosc_range) {
        result.request = REQUEST_NONE;
      } else {
        result.request = REQUEST_SET_HARMOSC_RANGE;
        result.argument[0] = loop_start;
        result.argument[1] = loop_end;
      }
      return result;
    } else {
      group_start = group_end = loop_start = loop_end;
    }
  }
  
  // The only situation where a loop can end on a patched channel is when
  // we have a single-channel group.
  if (group_end == loop_end && group_start != group_end) {
    // Correct:
    // LOOP     ---S--
    //             E
    // PATCHED  ---xx-
    
    // Incorrect:
    // LOOP     ---S-E
    // PATCHED  ---x-x
    result.request = REQUEST_NONE;  // inconsistent loop
    return result;
  }
  
  result.request = REQUEST_SET_LOOP;
  result.argument[0] = group_start;
  result.argument[1] = loop_start;
  result.argument[2] = loop_end;
  result.argument[3] = group_end;
  return result;
}

void ChainState::PollSwitches() {
  // The last module in the chain polls the states of the switches for the
  // entire chain. The state of the switches has been passed from left
  // to right.
  //
  // If a switch has been pressed, a Request packet is passed from right
  // to left. Each module is responsible from parsing the Request packet
  // and adjusting its internal state to simulate local changes. During
  // the next cycle (1ms later), the internal change will be propagated
  // from module to module through the usual mechanism (ChannelState
  // transmission).
  if (index_ == size_ - 1) {
    request_.request = REQUEST_NONE;
    size_t switch_index = 0;
    size_t first_pressed = kMaxNumChannels;

    for (size_t i = 0; i < size_; ++i) {
      ChannelBitmask switch_pressed = switch_pressed_[i];
      for (size_t j = 0; j < kNumChannels; ++j) {
        if (switch_pressed & 1) {
          if (switch_press_time_[switch_index] != -1) {
            ++switch_press_time_[switch_index];
            if (first_pressed != kMaxNumChannels) {
              // Simultaneously pressing a pair of buttons.
              request_ = MakeLoopChangeRequest(first_pressed, switch_index);
              if (request_.request == REQUEST_SET_HARMOSC_RANGE) {
                harmosc_start_or_end_[first_pressed] = !harmosc_start_or_end_[first_pressed];
                harmosc_start_or_end_[switch_index] = !harmosc_start_or_end_[switch_index];
              }
              switch_press_time_[first_pressed] = -1;
              switch_press_time_[switch_index] = -1;
            } else if (switch_press_time_[switch_index] > kLongPressDuration) {
              // Long press on a single button.
              request_ = MakeLoopChangeRequest(switch_index, switch_index);
              switch_press_time_[switch_index] = -1;
            } else {
              first_pressed = switch_index;
            }
          }
        } else {
          if (switch_press_time_[switch_index] > 5) {
            // A button has been released after having been held for a
            // sufficiently long time (5ms), but not for long enough to be
            // detected as a long press.
            request_.request = REQUEST_SET_SEGMENT_TYPE;
            request_.argument[0] = switch_index;
          }
          switch_press_time_[switch_index] = 0;
        }
        switch_pressed >>= 1;
        ++switch_index;
      }
    }

    if (request_.request != REQUEST_NONE) {
      return;
    }

    // If there are no other pending requests, use this opportunity to check if
    // any existing harmosc range or ranges need to be cancelled because of a
    // newly-patched gate input. If N ranges need to be cancelled the cancellations
    // will be spread over N separate requests.
    size_t channel_index = 0;
    bool open_harmosc_range = false;
    bool any_input_patched = false;
    size_t harmosc_block_start = -1;

    for (size_t i = 0; i < size_; ++i) {
      ChannelBitmask input_patched = input_patched_[i];
      for (size_t j = 0; j < kNumChannels; ++j) {
        if (input_patched & 1) {
          any_input_patched = true;
        }

        if (harmosc_start_or_end_[channel_index]) {
          if (!open_harmosc_range) {
            harmosc_block_start = channel_index;
          } else if (any_input_patched) {
            request_.request = REQUEST_SET_HARMOSC_RANGE;
            request_.argument[0] = harmosc_block_start;
            request_.argument[1] = channel_index;
            harmosc_start_or_end_[harmosc_block_start] = false;
            harmosc_start_or_end_[channel_index] = false;
            return;
          }
          open_harmosc_range = !open_harmosc_range;
        }

        input_patched >>= 1;
        ++channel_index;
      }
    }
  }
}

void ChainState::HandleRequest(Settings* settings) {
  if (request_.request == REQUEST_NONE) {
    return;
  }

  State* s = settings->mutable_state();
  bool dirty = false;
  for (size_t i = 0; i < kNumChannels; ++i) {
    size_t channel = local_channel_index(i);

    uint8_t type_bits = s->segment_configuration[i] & 0x3;
    uint8_t loop_bit = s->segment_configuration[i] & 0x4;
    uint8_t harmosc_bit = s->segment_configuration[i] & 0x8;

    if (request_.request == REQUEST_SET_SEGMENT_TYPE) {
      if (channel == request_.argument[0]) {
        s->segment_configuration[i] = ((type_bits + 1) % 3) | loop_bit | harmosc_bit;
        dirty |= true;
      }
    } else if (request_.request == REQUEST_SET_LOOP) {
      uint8_t new_loop_bit = loop_bit;
      if ((channel >= request_.argument[0] && channel < request_.argument[3])) {
        new_loop_bit = 0x0;
        // NOTE: everything in the range is cancelled by default, and only becomes a
        // new loop if it meets the later checks
      }
      if (channel == request_.argument[1] || channel == request_.argument[2]) {
        if (request_.argument[1] == request_.argument[2]) {
          // NOTE: this is how single-segment loops get cancelled
          new_loop_bit = 0x4 - loop_bit;
        } else {
          new_loop_bit = 0x4;
        }
      }
      s->segment_configuration[i] = type_bits | new_loop_bit | harmosc_bit;
      dirty |= new_loop_bit != loop_bit;
    } else if (request_.request == REQUEST_SET_HARMOSC_RANGE) {
      uint8_t new_type_bits = type_bits;
      uint8_t new_loop_bit = loop_bit;
      uint8_t new_harmosc_bit = harmosc_bit;

      if ((channel >= request_.argument[0] && channel < request_.argument[1])) {
        // Clear any single-segment loops in the new harmosc range.
        new_loop_bit = 0x0;
        // New harmosc ranges are created with all sine harmonics.
        new_type_bits = 0x0;
      }
      if (channel == request_.argument[0] || channel == request_.argument[1]) {
        new_harmosc_bit = 0x8 - harmosc_bit;
      }
      s->segment_configuration[i] = new_type_bits | new_loop_bit | new_harmosc_bit;
      dirty |= new_harmosc_bit != harmosc_bit;
    }
  }
  
  if (dirty) {
    settings->SaveState();
  }
}

void ChainState::Update(
    const IOBuffer::Block& block,
    Settings* settings,
    Oscillator* oscillator,
    SegmentGenerator* segment_generator,
    SegmentGenerator::Output* out) {
  if (discovering_neighbors_) {
    DiscoverNeighbors();
    return;
  }
  
  switch (counter_ & 0x3) {
    case 0:
      PollSwitches();
      UpdateLocalState(block, *settings, out[kBlockSize - 1], oscillator);
      TransmitRight();
      break;
    case 1:
      ReceiveRight();
      HandleRequest(settings);
      break;
    case 2:
      UpdateLocalPotCvSlider(block);
      TransmitLeft();
      break;
    case 3:
      ReceiveLeft();
      Configure(segment_generator, oscillator);
      BindRemoteParameters(segment_generator, oscillator);
      break;
  }
  
  BindLocalParameters(block, segment_generator, oscillator);
  fill(&out[0], &out[kBlockSize], rx_last_sample_);
  
  ++counter_;
}

}  // namespace stages
