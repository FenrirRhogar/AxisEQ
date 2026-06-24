#pragma once

#include <JuceHeader.h>

namespace ProEQ
{

/// Creates the complete APVTS parameter layout for the ProEQ plugin.
///
/// Registers 24 bands × 13 parameters = 312 total parameters with
/// proper ranges, skew factors, and human-readable string conversion
/// functions for host automation display.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace ProEQ
