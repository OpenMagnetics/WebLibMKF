#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "Constants.h"
#include "Definitions.h"
#include "constructive_models/Insulation.h"
#include "Defaults.h"
#include <MAS.hpp>
#include "constructive_models/Coil.h"
#include "constructive_models/NumberTurns.h"
#include "constructive_models/CorePiece.h"
#include "processors/MagneticSimulator.h"
#include "physical_models/WindingOhmicLosses.h"
#include "physical_models/WindingSkinEffectLosses.h"
#include "physical_models/WindingLosses.h"
#include "physical_models/Temperature.h"
#include "advisers/WireAdviser.h"
#include "advisers/CoilAdviser.h"
#include "advisers/CoreAdviser.h"
#include "advisers/MagneticAdviser.h"
#include "processors/Inputs.h"
#include "constructive_models/Core.h"
#include "physical_models/ComplexPermeability.h"
#include "physical_models/CoreLosses.h"
#include "physical_models/CoreTemperature.h"
#include "physical_models/InitialPermeability.h"
#include "physical_models/LeakageInductance.h"
#include "physical_models/MagnetizingInductance.h"
#include "physical_models/Inductance.h"
#include "physical_models/StrayCapacitance.h"
#include "physical_models/Reluctance.h"
#include "converter_models/CommonModeChoke.h"
#include "converter_models/Flyback.h"
#include "converter_models/IsolatedBuck.h"
#include "converter_models/IsolatedBuckBoost.h"
#include "converter_models/Buck.h"
#include "converter_models/Boost.h"
#include "converter_models/PushPull.h"
#include "converter_models/SingleSwitchForward.h"
#include "converter_models/PowerFactorCorrection.h"
#include "processors/NgspiceRunner.h"
#include "converter_models/ActiveClampForward.h"
#include "converter_models/TwoSwitchForward.h"
#include "converter_models/CommonModeChoke.h"
#include "converter_models/DifferentialModeChoke.h"
#include "converter_models/Llc.h"
#include "converter_models/Cllc.h"
#include "converter_models/Dab.h"
#include "converter_models/PhaseShiftedFullBridge.h"
#include "support/Painter.h"
#include "support/Utils.h"
#include "processors/Sweeper.h"
#include "processors/CircuitSimulatorInterface.h"
#include <magic_enum.hpp>


using namespace MAS;
using namespace emscripten;
using json = nlohmann::json;

// Forward declarations for waveform repetition helpers
void repeat_waveform_for_periods(std::vector<double>& time, std::vector<double>& data, size_t numberOfPeriods);
void repeat_operating_points_waveforms(json& operatingPoints, size_t numberOfPeriods);
void repeat_converter_waveforms_periods(json& converterWaveforms, size_t numberOfPeriods);
using ordered_json = nlohmann::ordered_json;

// Forward declarations for new converter processing functions
std::string process_converter(std::string topologyName, std::string converterJson, bool useNgspice);
std::string design_magnetics_from_converter(std::string topologyName, std::string converterJson, 
                                             int maxResults, std::string coreModeString, 
                                             bool useNgspice, std::string weightsString);

std::map<std::string, double> get_constants() {
    std::map<std::string, double> constantsMap;
    constantsMap["residualGap"] = OpenMagnetics::constants.residualGap;
    constantsMap["minimumNonResidualGap"] = OpenMagnetics::constants.minimumNonResidualGap;
    constantsMap["vacuumPermeability"] = OpenMagnetics::constants.vacuumPermeability;
    constantsMap["vacuumPermittivity"] = OpenMagnetics::constants.vacuumPermittivity;
    constantsMap["quasiStaticFrequencyLimit"] = OpenMagnetics::constants.quasiStaticFrequencyLimit;
    constantsMap["spacerProtudingPercentage"] = OpenMagnetics::constants.spacerProtudingPercentage;
    constantsMap["coilPainterScale"] = OpenMagnetics::constants.coilPainterScale;
    constantsMap["numberPointsSampledWaveforms"] = OpenMagnetics::constants.numberPointsSampledWaveforms;
    constantsMap["minimumDistributedFringingFactor"] = OpenMagnetics::constants.minimumDistributedFringingFactor;
    constantsMap["maximumDistributedFringingFactor"] = OpenMagnetics::constants.maximumDistributedFringingFactor;
    constantsMap["initialGapLengthForSearching"] = OpenMagnetics::constants.initialGapLengthForSearching;
    constantsMap["roshenMagneticFieldStrengthStep"] = OpenMagnetics::constants.roshenMagneticFieldStrengthStep;
    constantsMap["foilToSectionMargin"] = OpenMagnetics::constants.foilToSectionMargin;
    constantsMap["planarToSectionMargin"] = OpenMagnetics::constants.planarToSectionMargin;
    return constantsMap;
}

std::map<std::string, double> get_defaults() {
    std::map<std::string, double> defaultsMap;
    defaultsMap["maximumProportionMagneticFluxDensitySaturation"] = OpenMagnetics::defaults.maximumProportionMagneticFluxDensitySaturation;
    defaultsMap["coreAdviserFrequencyReference"] = OpenMagnetics::defaults.coreAdviserFrequencyReference;
    defaultsMap["coreAdviserMagneticFluxDensityReference"] = OpenMagnetics::defaults.coreAdviserMagneticFluxDensityReference;
    defaultsMap["coreAdviserThresholdValidity"] = OpenMagnetics::defaults.coreAdviserThresholdValidity;
    defaultsMap["coreAdviserMaximumCoreTemperature"] = OpenMagnetics::defaults.coreAdviserMaximumCoreTemperature;
    defaultsMap["coreAdviserMaximumPercentagePowerCoreLosses"] = OpenMagnetics::defaults.coreAdviserMaximumPercentagePowerCoreLosses;
    defaultsMap["coreAdviserMaximumMagneticsAfterFiltering"] = OpenMagnetics::defaults.coreAdviserMaximumMagneticsAfterFiltering;
    defaultsMap["coreAdviserMaximumNumberStacks"] = OpenMagnetics::defaults.coreAdviserMaximumNumberStacks;
    defaultsMap["maximumCurrentDensity"] = OpenMagnetics::defaults.maximumCurrentDensity;
    defaultsMap["maximumEffectiveCurrentDensity"] = OpenMagnetics::defaults.maximumEffectiveCurrentDensity;
    defaultsMap["maximumNumberParallels"] = OpenMagnetics::defaults.maximumNumberParallels;
    defaultsMap["magneticFluxDensitySaturation"] = OpenMagnetics::defaults.magneticFluxDensitySaturation;
    defaultsMap["magnetizingInductanceThresholdValidity"] = OpenMagnetics::defaults.magnetizingInductanceThresholdValidity;
    defaultsMap["harmonicAmplitudeThreshold"] = OpenMagnetics::defaults.harmonicAmplitudeThreshold;
    defaultsMap["ambientTemperature"] = OpenMagnetics::defaults.ambientTemperature;
    defaultsMap["measurementFrequency"] = OpenMagnetics::defaults.measurementFrequency;
    defaultsMap["magneticFieldMirroringDimension"] = OpenMagnetics::defaults.magneticFieldMirroringDimension;
    defaultsMap["maximumCoilPattern"] = OpenMagnetics::defaults.maximumCoilPattern;
    defaultsMap["overlappingFactorSurroundingTurns"] = OpenMagnetics::defaults.overlappingFactorSurroundingTurns;

    return defaultsMap;
}

std::string standardize_signal_descriptor(std::string signalDescriptorString, double frequency) {
    try {
        SignalDescriptor signalDescriptor(json::parse(signalDescriptorString));

        auto standardSignalDescriptor = OpenMagnetics::Inputs::standardize_waveform(signalDescriptor, frequency);
        if (standardSignalDescriptor.get_harmonics()) {
            auto processed = OpenMagnetics::Inputs::calculate_processed_data(standardSignalDescriptor.get_harmonics().value(), standardSignalDescriptor.get_waveform().value(), true);
            standardSignalDescriptor.set_processed(processed);
        }
        else {
            auto processed = OpenMagnetics::Inputs::calculate_processed_data(standardSignalDescriptor.get_waveform().value(), frequency, true);
            standardSignalDescriptor.set_processed(processed);
        }

        json result;
        to_json(result, standardSignalDescriptor);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<size_t> get_main_harmonic_indexes(std::string harmonicsString, double windingLossesHarmonicAmplitudeThreshold, int mainHarmonicIndex) {
    try {
        Harmonics harmonics;
        from_json(json::parse(harmonicsString), harmonics);

        std::vector<size_t> mainHarmonicIndexes;
        if (mainHarmonicIndex == -1) {
            mainHarmonicIndexes = OpenMagnetics::get_main_harmonic_indexes(harmonics, windingLossesHarmonicAmplitudeThreshold);
        }
        else {
            mainHarmonicIndexes = OpenMagnetics::get_main_harmonic_indexes(harmonics, windingLossesHarmonicAmplitudeThreshold, mainHarmonicIndex);
        }

        return mainHarmonicIndexes;
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::vector<size_t> get_excitation_harmonic_indexes(std::string excitationString, double windingLossesHarmonicAmplitudeThreshold) {
    try {
        OperatingPointExcitation excitation(json::parse(excitationString));

        auto mainHarmonicIndexes = OpenMagnetics::get_excitation_harmonic_indexes(excitation, windingLossesHarmonicAmplitudeThreshold);

        return mainHarmonicIndexes;
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::string calculate_harmonics(std::string waveformString, double frequency) {
    try {
        Waveform waveform;
        from_json(json::parse(waveformString), waveform);

        auto sampledCurrentWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(waveform, frequency);
        auto harmonics = OpenMagnetics::Inputs::calculate_harmonics_data(sampledCurrentWaveform, frequency);

        json result;
        to_json(result, harmonics);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_processed(std::string harmonicsString, std::string waveformString) {
    try {
        Waveform waveform;
        Harmonics harmonics;
        from_json(json::parse(waveformString), waveform);
        from_json(json::parse(harmonicsString), harmonics);

        auto processed = OpenMagnetics::Inputs::calculate_processed_data(harmonics, waveform, true);

        json result;
        to_json(result, processed);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << harmonicsString << std::endl;
        std::cerr << waveformString << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_core_data_from_shape(std::string shapeString){
    try {
        CoreShape shape(json::parse(shapeString));
        OpenMagnetics::Core core;
        CoreFunctionalDescription coreFunctionalDescription;
        coreFunctionalDescription.set_shape(shape);
        coreFunctionalDescription.set_material("Dummy");
        coreFunctionalDescription.set_number_stacks(1);
        if (shape.get_magnetic_circuit() == MagneticCircuit::OPEN) {
            coreFunctionalDescription.set_type(CoreType::TWO_PIECE_SET);
        }
        else {
            if (shape.get_family() == CoreShapeFamily::T) {
                coreFunctionalDescription.set_type(CoreType::TOROIDAL);
            }
            else {
                coreFunctionalDescription.set_type(CoreType::CLOSED_SHAPE);
            }
        }
        core.set_functional_description(coreFunctionalDescription);
        core.process_data();

        json result;
        to_json(result, core);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_all_core_data_from_shapes(){
    try {
        // Get all shapes from the database
        auto allShapes = OpenMagnetics::get_shapes(true);
        
        json resultArray = json::array();
        
        for (const auto& shape : allShapes) {
            try {
                OpenMagnetics::Core core;
                CoreFunctionalDescription coreFunctionalDescription;
                coreFunctionalDescription.set_shape(shape);
                coreFunctionalDescription.set_material("Dummy");
                coreFunctionalDescription.set_number_stacks(1);
                if (shape.get_magnetic_circuit() == MagneticCircuit::OPEN) {
                    coreFunctionalDescription.set_type(CoreType::TWO_PIECE_SET);
                }
                else {
                    if (shape.get_family() == CoreShapeFamily::T) {
                        coreFunctionalDescription.set_type(CoreType::TOROIDAL);
                    }
                    else {
                        coreFunctionalDescription.set_type(CoreType::CLOSED_SHAPE);
                    }
                }
                core.set_functional_description(coreFunctionalDescription);
                core.process_data();
                
                json coreJson;
                to_json(coreJson, core);
                resultArray.push_back(coreJson);
            }
            catch (const std::exception &exc) {
                // Skip shapes that fail to process, log if needed
                // std::cerr << "Failed to process shape: " << exc.what() << std::endl;
            }
        }
        
        return resultArray.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    try {
        OpenMagnetics::Core core(json::parse(coreDataString), includeMaterialData, true);

        json result;
        to_json(result, core);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_bobbin_data(std::string magneticString){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        auto optionalBobbin = magnetic.get_coil().get_bobbin();
        OpenMagnetics::Bobbin bobbin;

        if (std::holds_alternative<std::string>(optionalBobbin)) {
            auto bobbinString = std::get<std::string>(optionalBobbin);
            if (bobbinString == "Dummy") {
                return "Exception: " + std::string{"Use create_simple_bobbin_from_core instead"};
            }
            else {
                bobbin = OpenMagnetics::find_bobbin_by_name(bobbinString);
            }
        }
        else {
            bobbin = OpenMagnetics::Bobbin(std::get<std::string>(optionalBobbin));
            bobbin.process_data();
        }

        json result;
        to_json(result, bobbin);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string create_simple_bobbin_from_core(std::string coresString){
    try {
        OpenMagnetics::Core core(json::parse(coresString), false, true);
        auto bobbin = OpenMagnetics::Bobbin::create_quick_bobbin(core);

        json result;
        to_json(result, bobbin);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string create_simple_bobbin_from_core_with_custom_thickness(std::string coresString, double thickness){
    try {
        OpenMagnetics::Core core(json::parse(coresString), false, true);
        auto bobbin = OpenMagnetics::Bobbin::create_quick_bobbin(core, thickness);

        json result;
        to_json(result, bobbin);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string create_simple_bobbin_from_core_with_custom_thicknesses(std::string coresString, double wallThickness, double columnThickness){
    try {
        OpenMagnetics::Core core(json::parse(coresString), false, true);
        auto bobbin = OpenMagnetics::Bobbin::create_quick_bobbin(core, wallThickness, columnThickness);

        json result;
        to_json(result, bobbin);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


std::string get_wire_data(std::string windingDataString){
    try {
        OpenMagnetics::Winding winding(json::parse(windingDataString));
        auto wire = OpenMagnetics::Coil::resolve_wire(winding);
        json result;
        to_json(result, wire);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_wire_data_by_name(std::string name){
    try {
        auto wireData = OpenMagnetics::find_wire_by_name(name);
        json result;
        to_json(result, wireData);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_wire_data_by_standard_name(std::string standardName){
    auto wires = OpenMagnetics::get_wires();
    for (auto wire : wires) {
        if (!wire.get_standard_name()) {
            continue;
        }
        if (wire.get_standard_name().value() == standardName) {
            auto coating = wire.resolve_coating();
            if (!coating) {
                continue;
            }
            if (!coating->get_grade()) {
                continue;
            }
            // // Hardcoded
            if (coating->get_grade().value() == 1) {
                json result;
                to_json(result, wire);
                return result.dump(4);
            }
        }
    }
    return "{}";
}

std::string get_planar_wire_by_standard_name(std::string standardName){
    try {
        // Normalize input: add period if missing (e.g., "2 oz" -> "2 oz.")
        std::string normalizedName = standardName;
        if (!normalizedName.empty() && normalizedName.back() != '.') {
            normalizedName += '.';
        }
        
        auto wires = OpenMagnetics::get_wires(WireType::PLANAR);
        for (auto wire : wires) {
            if (!wire.get_standard_name()) {
                continue;
            }
            std::string wireStandardName = wire.get_standard_name().value();
            // Try matching both normalized and original
            if (wireStandardName == standardName || wireStandardName == normalizedName) {
                json result;
                to_json(result, wire);
                return result.dump(4);
            }
        }
        return "{}";
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Helper function to repeat waveform data for multiple periods
void repeat_waveform_for_periods(std::vector<double>& time, std::vector<double>& data, size_t numberOfPeriods) {
    if (numberOfPeriods <= 1 || time.empty() || data.empty()) {
        return;
    }
    
    double period = time.back() - time.front();
    size_t originalSize = time.size();
    
    // Reserve space for repeated data
    time.reserve(originalSize * numberOfPeriods);
    data.reserve(originalSize * numberOfPeriods);
    
    for (size_t p = 1; p < numberOfPeriods; ++p) {
        double offset = p * period;
        for (size_t i = 0; i < originalSize; ++i) {
            // Skip first point of subsequent periods to avoid duplicates
            if (i == 0) continue;
            time.push_back(time[i] + offset);
            data.push_back(data[i]);
        }
    }
}

// Helper function to repeat all waveforms in operating points
void repeat_operating_points_waveforms(json& operatingPoints, size_t numberOfPeriods) {
    if (numberOfPeriods <= 1 || !operatingPoints.is_array()) {
        return;
    }
    
    for (auto& op : operatingPoints) {
        if (!op.contains("excitationsPerWinding") || !op["excitationsPerWinding"].is_array()) {
            continue;
        }
        
        for (auto& excitation : op["excitationsPerWinding"]) {
            // Repeat voltage waveform
            if (excitation.contains("voltage") && excitation["voltage"].contains("waveform")) {
                auto& waveform = excitation["voltage"]["waveform"];
                if (waveform.contains("time") && waveform.contains("data")) {
                    auto time = waveform["time"].get<std::vector<double>>();
                    auto data = waveform["data"].get<std::vector<double>>();
                    repeat_waveform_for_periods(time, data, numberOfPeriods);
                    waveform["time"] = time;
                    waveform["data"] = data;
                }
            }
            
            // Repeat current waveform
            if (excitation.contains("current") && excitation["current"].contains("waveform")) {
                auto& waveform = excitation["current"]["waveform"];
                if (waveform.contains("time") && waveform.contains("data")) {
                    auto time = waveform["time"].get<std::vector<double>>();
                    auto data = waveform["data"].get<std::vector<double>>();
                    repeat_waveform_for_periods(time, data, numberOfPeriods);
                    waveform["time"] = time;
                    waveform["data"] = data;
                }
            }
        }
    }
}

// Helper function to repeat all waveforms in converter waveforms
void repeat_converter_waveforms_periods(json& converterWaveforms, size_t numberOfPeriods) {
    if (numberOfPeriods <= 1 || !converterWaveforms.is_array()) {
        return;
    }
    
    for (auto& cw : converterWaveforms) {
        // Repeat input voltage waveform
        if (cw.contains("inputVoltage") && cw["inputVoltage"].contains("time") && cw["inputVoltage"].contains("data")) {
            auto time = cw["inputVoltage"]["time"].get<std::vector<double>>();
            auto data = cw["inputVoltage"]["data"].get<std::vector<double>>();
            repeat_waveform_for_periods(time, data, numberOfPeriods);
            cw["inputVoltage"]["time"] = time;
            cw["inputVoltage"]["data"] = data;
        }
        
        // Repeat input current waveform
        if (cw.contains("inputCurrent") && cw["inputCurrent"].contains("time") && cw["inputCurrent"].contains("data")) {
            auto time = cw["inputCurrent"]["time"].get<std::vector<double>>();
            auto data = cw["inputCurrent"]["data"].get<std::vector<double>>();
            repeat_waveform_for_periods(time, data, numberOfPeriods);
            cw["inputCurrent"]["time"] = time;
            cw["inputCurrent"]["data"] = data;
        }
        
        // Repeat output voltages
        if (cw.contains("outputVoltages") && cw["outputVoltages"].is_array()) {
            for (auto& outV : cw["outputVoltages"]) {
                if (outV.contains("time") && outV.contains("data")) {
                    auto time = outV["time"].get<std::vector<double>>();
                    auto data = outV["data"].get<std::vector<double>>();
                    repeat_waveform_for_periods(time, data, numberOfPeriods);
                    outV["time"] = time;
                    outV["data"] = data;
                }
            }
        }
        
        // Repeat output currents
        if (cw.contains("outputCurrents") && cw["outputCurrents"].is_array()) {
            for (auto& outI : cw["outputCurrents"]) {
                if (outI.contains("time") && outI.contains("data")) {
                    auto time = outI["time"].get<std::vector<double>>();
                    auto data = outI["data"].get<std::vector<double>>();
                    repeat_waveform_for_periods(time, data, numberOfPeriods);
                    outI["time"] = time;
                    outI["data"] = data;
                }
            }
        }
    }
}

double get_wire_outer_width_rectangular(double conductingWidth, int grade, std::string wireStandardString){
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_width_rectangular(conductingWidth, grade, wireStandard);
}

double get_wire_outer_height_rectangular(double conductingHeight, int grade, std::string wireStandardString){
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_height_rectangular(conductingHeight, grade, wireStandard);
}

double get_wire_outer_diameter_bare_litz(double conductingDiameter, int numberConductors, int grade, std::string wireStandardString) {
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_diameter_bare_litz(conductingDiameter, numberConductors, grade, wireStandard);
}

double get_wire_outer_diameter_served_litz(double conductingDiameter, int numberConductors, int grade, int numberLayers, std::string wireStandardString) {
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_diameter_served_litz(conductingDiameter, numberConductors, grade, numberLayers, wireStandard);
}

double get_wire_outer_diameter_insulated_litz(double conductingDiameter, int numberConductors, int numberLayers, double thicknessLayers, int grade, std::string wireStandardString) {
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_diameter_insulated_litz(conductingDiameter, numberConductors, numberLayers, thicknessLayers, grade, wireStandard);
}

double get_wire_outer_diameter_enamelled_round(double conductingDiameter, int grade, std::string wireStandardString) {
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_diameter_round(conductingDiameter, grade, wireStandard);
}

double get_wire_outer_diameter_insulated_round(double conductingDiameter, int numberLayers, double thicknessLayers, std::string wireStandardString) {
    WireStandard wireStandard;
    from_json(wireStandardString, wireStandard);
    return OpenMagnetics::Wire::get_outer_diameter_round(conductingDiameter, numberLayers, thicknessLayers, wireStandard);
}

std::vector<double> get_outer_dimensions(std::string wireString) {
    OpenMagnetics::Wire wire(json::parse(wireString));
    return {wire.get_maximum_outer_width(), wire.get_maximum_outer_height()};
}


std::string get_strand_by_standard_name(std::string standardName){
    auto wires = OpenMagnetics::get_wires();
    for (auto wire : wires) {
        if (!wire.get_standard_name()) {
            continue;
        }
        auto coating = wire.resolve_coating();
        if (!coating) {
            continue;
        }
        // We are looking for enamelled wires for strands
        if (coating->get_type() != InsulationWireCoatingType::ENAMELLED) {
            continue;
        }

        if (!coating->get_grade()) {
            throw std::runtime_error("Missing grade");
        }

        if (wire.get_standard_name().value() == standardName && coating->get_grade().value() == 1) {
            json result;
            to_json(result, wire);
            return result.dump(4);
        }
    }

    json result;
    result["errorMessage"] = "Wire not found by standard name";
    return result.dump(4);
}

double get_wire_conducting_diameter_by_standard_name(std::string standardName){
    auto wires = OpenMagnetics::get_wires();
    for (auto wire : wires) {
        if (!wire.get_standard_name()) {
            continue;
        }
        if (wire.get_standard_name().value() == standardName) {
            return OpenMagnetics::resolve_dimensional_values(wire.get_conducting_diameter().value());
        }
    }

    return -1;
}


std::string get_equivalent_wire(std::string oldWireString, std::string newWireTypeString, double effectivefrequency){
    try {
        OpenMagnetics::Wire oldWire(json::parse(oldWireString));
        WireType newWireType;
        from_json(json::parse(newWireTypeString), newWireType);

        auto newWire = OpenMagnetics::Wire::get_equivalent_wire(oldWire, newWireType, effectivefrequency);

        json result;
        to_json(result, newWire);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;

        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_coating_label(std::string wireString){
    try {
        OpenMagnetics::Wire wire(json::parse(wireString));
        auto coatingLabel = wire.encode_coating_label();
        return coatingLabel;
    }
    catch(const std::runtime_error& re)
    {
        return "Exception: " + std::string{re.what()};
    }
    catch(const std::exception& ex)
    {
        return "Exception: " + std::string{ex.what()};
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string get_wire_coating_by_label(std::string label){
    try {
        auto wires = OpenMagnetics::get_wires();
        InsulationWireCoating insulationWireCoating;
        for (auto wire : wires) {
            auto coatingLabel = wire.encode_coating_label();
            if (coatingLabel == label) {
                if (wire.resolve_coating()) {
                    insulationWireCoating = wire.resolve_coating().value();
                }
                else {
                    insulationWireCoating.set_type(InsulationWireCoatingType::BARE);
                }
                break;
            }
        }
        json result;
        to_json(result, insulationWireCoating);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<std::string> get_coating_labels_by_type(std::string wireTypeString){
    WireType wireType(json::parse(wireTypeString));

    auto wires = OpenMagnetics::get_wires(wireType);

    std::vector<std::string> coatingLabels;
    for (auto wire : wires) {
        auto coatingLabel = wire.encode_coating_label();
        if (std::find(coatingLabels.begin(), coatingLabels.end(), coatingLabel) == coatingLabels.end()) {
            coatingLabels.push_back(coatingLabel);
        }
    }

    return coatingLabels;
}

std::string load_core_data(std::string coresString){
    try {
        json result = json::array();
        for (auto& coreJson : json::parse(coresString)) {
            OpenMagnetics::Core core(coreJson, false);
            json aux;
            to_json(aux, core);
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_material_data(std::string materialName){
    try {
        auto materialData = OpenMagnetics::find_core_material_by_name(materialName);
        json result;
        to_json(result, materialData);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_core_temperature_dependant_parameters(std::string coreData, double temperature){
    try {
        json coreJson = json::parse(coreData);
        MAS::CoreFunctionalDescription coreFunctionalDescription(coreJson["functionalDescription"]);
        MAS::CoreProcessedDescription coreProcessedDescription(coreJson["processedDescription"]);
        OpenMagnetics::Core core;
        core.set_functional_description(coreFunctionalDescription);
        core.set_processed_description(coreProcessedDescription);
        
        // Handle null or missing geometricalDescription
        if (coreJson.contains("geometricalDescription") && !coreJson["geometricalDescription"].is_null()) {
            std::vector<MAS::CoreGeometricalDescriptionElement> coreGeometricalDescription(coreJson["geometricalDescription"]);
            core.set_geometrical_description(coreGeometricalDescription);
        } else {
            core.set_geometrical_description(std::nullopt);
        }
        
        json result;

        result["magneticFluxDensitySaturation"] = core.get_magnetic_flux_density_saturation(temperature, false);
        result["magneticFieldStrengthSaturation"] = core.get_magnetic_field_strength_saturation(temperature);
        result["initialPermeability"] = core.get_initial_permeability(temperature);
        result["effectivePermeability"] = core.get_effective_permeability(temperature);
        result["reluctance"] = core.get_reluctance(temperature);
        auto reluctanceModel = OpenMagnetics::ReluctanceModel::factory();
        result["permeance"] = 1.0 / reluctanceModel->get_ungapped_core_reluctance(core);
        result["resistivity"] = core.get_resistivity(temperature);

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_shape_data(std::string shapeName){
    try {
        auto shapeData = OpenMagnetics::find_core_shape_by_name(shapeName);

        json result;
        to_json(result, shapeData);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<std::string> get_available_core_shape_families(){
    std::vector<std::string> families;
    for (auto& family : OpenMagnetics::get_core_shape_families()) {
        json familyJson;
        to_json(familyJson, family);
        families.push_back(familyJson);
    }
    return families;
}

std::vector<std::string> get_available_core_manufacturers(){
    std::vector<std::string> manufacturers;
    auto materials = OpenMagnetics::get_materials("");
    for (auto material : materials) {
        std::string manufacturer = material.get_manufacturer_info().get_name();
        if (std::find(manufacturers.begin(), manufacturers.end(), manufacturer) == manufacturers.end()) {
            manufacturers.push_back(manufacturer);
        }
    }
    return manufacturers;
}

std::vector<std::string> get_available_core_materials(std::string manufacturer){
    return OpenMagnetics::get_core_material_names(manufacturer);
}

std::vector<std::string> get_available_core_shapes(){
    return OpenMagnetics::get_core_shape_names();
}

std::vector<std::string> get_available_core_shapes_by_manufacturer(std::string manufacturer){
    return OpenMagnetics::get_core_shape_names(manufacturer);
}

std::vector<std::string> get_available_core_shapes_by_family(std::string familyString){
    try {
        CoreShapeFamily family;
        from_json(familyString, family);
         
        return OpenMagnetics::get_core_shape_names(family);
    }
    catch (const std::exception &exc) {
        return {"Exception: " + std::string{exc.what()}};
    }
}

std::vector<std::string> get_shape_family_dimensions(std::string familyString, std::string familySubtype) {
    try {
        CoreShapeFamily family;
        from_json(familyString, family);

        std::vector<std::string> dimensions;
        if (familySubtype != "") {
            dimensions = OpenMagnetics::get_shape_family_dimensions(family, familySubtype);
        }
        else {
            dimensions = OpenMagnetics::get_shape_family_dimensions(family);
        }
         
        return dimensions;
    }
    catch (const std::exception &exc) {
        return {"Exception: " + std::string{exc.what()}};
    }
}

std::vector<std::string> get_shape_family_subtypes(std::string familyString) {
    try {
        CoreShapeFamily family;
        from_json(familyString, family);

        std::vector<std::string> familySubtypes;
        familySubtypes = OpenMagnetics::get_shape_family_subtypes(family);

        return familySubtypes;
    }
    catch (const std::exception &exc) {
        return {"Exception: " + std::string{exc.what()}};
    }
}

std::vector<std::string> get_available_wires(){
    return OpenMagnetics::get_wire_names();
}

std::vector<std::string> get_unique_wire_diameters(std::string wireStandardString){
    try {
        WireStandard wireStandard(json::parse(wireStandardString));
        std::vector<std::string> uniqueStandardName;

        {
            auto wires = OpenMagnetics::get_wires(WireType::ROUND, wireStandard);

            for (auto wire : wires) {
                if (!wire.get_standard_name()) {
                    continue;
                }
                auto standardName = wire.get_standard_name().value();
                if (std::find(uniqueStandardName.begin(), uniqueStandardName.end(), standardName) == uniqueStandardName.end()) {
                    uniqueStandardName.push_back(standardName);
                }
            }
        }

        {
            auto wires = OpenMagnetics::get_wires(WireType::LITZ, wireStandard);

            for (auto wire : wires) {
                auto strand = wire.resolve_strand();

                auto strandStandardName = strand.get_standard_name().value();
                if (std::find(uniqueStandardName.begin(), uniqueStandardName.end(), strandStandardName) == uniqueStandardName.end()) {
                    uniqueStandardName.push_back(strandStandardName);
                }
            }
        }
        return uniqueStandardName;
    }
    catch (const std::exception &exc) {
        return {"Exception: " + std::string{exc.what()}};
    }
}


std::vector<std::string> get_planar_thicknesses(){
    try {
        std::vector<std::string> uniqueStandardName;
        auto wires = OpenMagnetics::get_wires(WireType::PLANAR);

        for (auto wire : wires) {

            auto standardName = wire.get_standard_name().value();
            if (std::find(uniqueStandardName.begin(), uniqueStandardName.end(), standardName) == uniqueStandardName.end()) {
                uniqueStandardName.push_back(standardName);
            }
        }
        return uniqueStandardName;
    }
    catch (const std::exception &exc) {
        return {"Exception: " + std::string{exc.what()}};
    }
}

std::vector<std::string> get_available_wire_types(){
    // std::vector<std::string> wireTypes;

    // for (auto [value, name] : magic_enum::enum_entries<WireType>()) {
    //     json wireTypeString;
    //     if (value == WireType::PLANAR) {
    //         // TODO Add support for planar
    //         continue;
    //     }
    //     to_json(wireTypeString, value);
    //     wireTypes.push_back(wireTypeString);
    // }
    std::vector<MAS::WireType> wireTypes;
    for (auto [reference, wire] : OpenMagnetics::wireDatabase) {
        auto wireType = wire.get_type();
        if (wireType == WireType::PLANAR) {
            // TODO Add support for planar
            continue;
        }
        if (std::find(wireTypes.begin(), wireTypes.end(), wireType) == wireTypes.end()) {
            wireTypes.push_back(wireType);
        }
    }
    std::vector<std::string> wireTypesString;
    for (auto wireType : wireTypes) {
        json wireTypeString;        
        to_json(wireTypeString, wireType);
        wireTypesString.push_back(wireTypeString);
    }

    return wireTypesString;
}

std::vector<std::string> get_available_wire_standards(){
    // std::vector<std::string> wireStandards;

    // for (auto [value, name] : magic_enum::enum_entries<WireStandard>()) {
    //     json wireStandardString;
    //     to_json(wireStandardString, value);
    //     wireStandards.push_back(wireStandardString);
    // }

    std::vector<MAS::WireStandard> wireStandards;
    for (auto [reference, wire] : OpenMagnetics::wireDatabase) {
        if (!wire.get_standard()) {
            continue;
        }
        auto wireStandard = wire.get_standard().value();
        if (std::find(wireStandards.begin(), wireStandards.end(), wireStandard) == wireStandards.end()) {
            wireStandards.push_back(wireStandard);
        }
    }
    std::vector<std::string> wireStandardsString;
    for (auto wireStandard : wireStandards) {
        json wireStandardString;        
        to_json(wireStandardString, wireStandard);
        wireStandardsString.push_back(wireStandardString);
    }
    return wireStandardsString;
}

std::string calculate_gap_reluctance(std::string coreGapData, std::string modelNameString){
    try {
        OpenMagnetics::ReluctanceModels modelName;
        from_json(modelNameString, modelName);
        auto reluctanceModel = OpenMagnetics::ReluctanceModel::factory(modelName);

        CoreGap coreGap(json::parse(coreGapData));

        auto coreGapResult = reluctanceModel->get_gap_reluctance(coreGap);
        json result;
        to_json(result, coreGapResult);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_gap_reluctance_model_information(){
    try {
        json info;
        info["information"] = OpenMagnetics::ReluctanceModel::get_models_information();
        info["errors"] = OpenMagnetics::ReluctanceModel::get_models_errors();
        info["internal_links"] = OpenMagnetics::ReluctanceModel::get_models_internal_links();
        info["external_links"] = OpenMagnetics::ReluctanceModel::get_models_external_links();
        return info.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

double calculate_inductance_from_number_turns_and_gapping(std::string coreData,
                                                          std::string coilData,
                                                          std::string operatingPointData,
                                                          std::string modelsData){
    try {
        json coreJson = json::parse(coreData);
        MAS::CoreFunctionalDescription coreFunctionalDescription(coreJson["functionalDescription"]);
        MAS::CoreProcessedDescription coreProcessedDescription(coreJson["processedDescription"]);
        std::vector<MAS::CoreGeometricalDescriptionElement> coreGeometricalDescription(coreJson["geometricalDescription"]);
        OpenMagnetics::Core core;
        core.set_functional_description(coreFunctionalDescription);
        core.set_processed_description(coreProcessedDescription);
        core.set_geometrical_description(coreGeometricalDescription);
        OpenMagnetics::Coil coil(json::parse(coilData), false);

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
        double magnetizingInductance;
        if (operatingPointData != "") {
            OperatingPoint operatingPoint(json::parse(operatingPointData));
            magnetizingInductance = magnetizingInductanceObj.calculate_inductance_from_number_turns_and_gapping(core, coil, &operatingPoint).get_magnetizing_inductance().get_nominal().value();
        }
        else {
            magnetizingInductance = magnetizingInductanceObj.calculate_inductance_from_number_turns_and_gapping(core, coil).get_magnetizing_inductance().get_nominal().value();
        }

        return magnetizingInductance;
    }
    catch (const std::exception &exc) {
        std::cerr << coreData << std::endl;
        std::cerr << coilData << std::endl;
        std::cerr << operatingPointData << std::endl;
        std::cerr << modelsData << std::endl;
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
        return -1;
    }
}


double calculate_number_turns_from_gapping_and_inductance(std::string coreData,
                                                          std::string inputsData,    
                                                          std::string modelsData){
    try {
        OpenMagnetics::Core core(json::parse(coreData));
        OpenMagnetics::Inputs inputs(json::parse(inputsData));

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
        double numberTurns = magnetizingInductanceObj.calculate_number_turns_from_gapping_and_inductance(core, &inputs);

        return numberTurns;
    }
    catch (const std::exception &exc) {
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
        return -1;
    }
}


std::string calculate_gapping_from_number_turns_and_inductance(std::string coreData,
                                                               std::string coilData,
                                                               std::string inputsData,
                                                               std::string gappingTypeString,
                                                               int decimals,
                                                               std::string modelsData){
    try {
        OpenMagnetics::Core core(json::parse(coreData));
        OpenMagnetics::Coil coil(json::parse(coilData), false);
        OpenMagnetics::Inputs inputs(json::parse(inputsData));

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        OpenMagnetics::GappingType gappingType;
        OpenMagnetics::from_json(gappingTypeString, gappingType);
        
        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
        std::vector<CoreGap> gapping = magnetizingInductanceObj.calculate_gapping_from_number_turns_and_inductance(core,
                                                                                                           coil,
                                                                                                           &inputs,
                                                                                                           gappingType,
                                                                                                           decimals);

        core.set_processed_description(std::nullopt);
        core.set_geometrical_description(std::nullopt);
        core.get_mutable_functional_description().set_gapping(gapping);
        core.process_data();
        core.process_gap();
        auto geometricalDescription = core.create_geometrical_description();
        core.set_geometrical_description(geometricalDescription);

        json result;
        to_json(result, core);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_core_losses(std::string coreData,
                                  std::string coilData,
                                  std::string inputsData,    
                                  std::string modelsData,    
                                  int operatingPointIndex){
    try {
        json coreJson = json::parse(coreData);
        MAS::CoreFunctionalDescription coreFunctionalDescription(coreJson["functionalDescription"]);
        MAS::CoreProcessedDescription coreProcessedDescription(coreJson["processedDescription"]);
        std::vector<MAS::CoreGeometricalDescriptionElement> coreGeometricalDescription(coreJson["geometricalDescription"]);
        OpenMagnetics::Core core;
        core.set_functional_description(coreFunctionalDescription);
        core.set_processed_description(coreProcessedDescription);
        core.set_geometrical_description(coreGeometricalDescription);
        OpenMagnetics::Coil coil(json::parse(coilData), false);

        OpenMagnetics::MagnetizingInductance magnetizingInductanceModel;
        double magnetizingInductance = magnetizingInductanceModel.calculate_inductance_from_number_turns_and_gapping(core, coil).get_magnetizing_inductance().get_nominal().value();

        OpenMagnetics::Inputs inputs(json::parse(inputsData), true, magnetizingInductance);
        auto operatingPoint = inputs.get_operating_point(operatingPointIndex);
        OperatingPointExcitation excitation = operatingPoint.get_excitations_per_winding()[0];
        // double magnetizingInductance = OpenMagnetics::resolve_dimensional_values(inputs.get_design_requirements().get_magnetizing_inductance());
        if (!excitation.get_current()) {
            auto magnetizingCurrent = OpenMagnetics::Inputs::calculate_magnetizing_current(excitation, magnetizingInductance, true, 0.0);
            excitation.set_current(magnetizingCurrent);
            operatingPoint.get_mutable_excitations_per_winding()[0] = excitation;
        }

        auto defaults = OpenMagnetics::Defaults();

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

        auto reluctanceModelName = OpenMagnetics::defaults.reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }
        auto coreLossesModelName = OpenMagnetics::defaults.coreLossesModelDefault;
        if (models.find("coreLosses") != models.end()) {
            OpenMagnetics::from_json(models["coreLosses"], coreLossesModelName);
        }
        auto coreTemperatureModelName = OpenMagnetics::defaults.coreTemperatureModelDefault;
        if (models.find("coreTemperature") != models.end()) {
            OpenMagnetics::from_json(models["coreTemperature"], coreTemperatureModelName);
        }

        OpenMagnetics::Magnetic magnetic;
        magnetic.set_core(core);
        magnetic.set_coil(coil);

        OpenMagnetics::MagneticSimulator magneticSimulator;
        magneticSimulator.set_core_losses_model_name(coreLossesModelName);
        magneticSimulator.set_core_temperature_model_name(coreTemperatureModelName);
        magneticSimulator.set_reluctance_model_name(reluctanceModelName);
        auto coreLossesOutput = magneticSimulator.calculate_core_losses(operatingPoint, magnetic);
        json result;
        to_json(result, coreLossesOutput);

        OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
        auto magneticFluxDensity = magnetizingInductanceObj.calculate_inductance_and_magnetic_flux_density(core, coil, &operatingPoint).second;
        excitation.set_magnetic_flux_density(magneticFluxDensity);

        result["magneticFluxDensityPeak"] = magneticFluxDensity.get_processed().value().get_peak().value();

        double frequency = OpenMagnetics::Inputs::get_switching_frequency(excitation);
        double magneticFluxDensityAcPeakToPeak = OpenMagnetics::Inputs::get_magnetic_flux_density_peak_to_peak(excitation, frequency);
        result["magneticFluxDensityAcPeak"] = magneticFluxDensityAcPeakToPeak / 2;
        result["voltageRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value();
        result["currentRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
        result["apparentPower"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value() * operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
        if (coreLossesOutput.get_temperature()) {
            result["maximumCoreTemperature"] = coreLossesOutput.get_temperature().value();
            result["maximumCoreTemperatureRise"] = coreLossesOutput.get_temperature().value() - operatingPoint.get_conditions().get_ambient_temperature();
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << coreData << std::endl;
        std::cerr << coilData << std::endl;
        std::cerr << inputsData << std::endl;
        std::cerr << modelsData << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_core_losses_model_information(std::string material){
    try {
        json info;
        info["information"] = OpenMagnetics::CoreLossesModel::get_models_information();
        info["errors"] = OpenMagnetics::CoreLossesModel::get_models_errors();
        info["internal_links"] = OpenMagnetics::CoreLossesModel::get_models_internal_links();
        info["external_links"] = OpenMagnetics::CoreLossesModel::get_models_external_links();
        info["available_models"] = OpenMagnetics::CoreLossesModel::get_methods_string(material);
        return info.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_core_temperature_model_information(){
    try {
        json info;
        info["information"] = OpenMagnetics::CoreTemperatureModel::get_models_information();
        info["errors"] = OpenMagnetics::CoreTemperatureModel::get_models_errors();
        info["internal_links"] = OpenMagnetics::CoreTemperatureModel::get_models_internal_links();
        info["external_links"] = OpenMagnetics::CoreTemperatureModel::get_models_external_links();
        return info.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_induced_voltage(std::string excitationString, double magnetizingInductance){
    try {
        OperatingPointExcitation excitation(json::parse(excitationString));

        auto voltage = OpenMagnetics::Inputs::calculate_induced_voltage(excitation, magnetizingInductance);

        json result;
        to_json(result, voltage);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_induced_current(std::string excitationString, double magnetizingInductance){
    try {
        OperatingPointExcitation excitation(json::parse(excitationString));

        auto current = OpenMagnetics::Inputs::calculate_magnetizing_current(excitation, magnetizingInductance, true, 0.0);

        if (excitation.get_voltage()) {
            if (excitation.get_voltage().value().get_processed()) {
                if (excitation.get_voltage().value().get_processed().value().get_duty_cycle()) {
                    auto processed = current.get_processed().value();
                    processed.set_duty_cycle(excitation.get_voltage().value().get_processed().value().get_duty_cycle().value());
                    current.set_processed(processed);
                }
            }
        }

        json result;
        to_json(result, current);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_reflected_secondary(std::string primaryExcitationString, double turnRatio){
    try {
        OperatingPointExcitation primaryExcitation(json::parse(primaryExcitationString));

        OperatingPointExcitation excitationOfThisWinding(primaryExcitation);
        auto currentSignalDescriptorProcessed = OpenMagnetics::Inputs::calculate_basic_processed_data(primaryExcitation.get_current().value().get_waveform().value());
        auto voltageSignalDescriptorProcessed = OpenMagnetics::Inputs::calculate_basic_processed_data(primaryExcitation.get_voltage().value().get_waveform().value());

        auto voltageSignalDescriptor = OpenMagnetics::Inputs::reflect_waveform(primaryExcitation.get_voltage().value(), 1.0 / turnRatio, voltageSignalDescriptorProcessed.get_label());
        auto currentSignalDescriptor = OpenMagnetics::Inputs::reflect_waveform(primaryExcitation.get_current().value(), turnRatio, currentSignalDescriptorProcessed.get_label());

        auto voltageSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
        voltageSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(voltageSampledWaveform, excitationOfThisWinding.get_frequency()));
        voltageSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));

        auto currentSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
        currentSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
        currentSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));

        excitationOfThisWinding.set_voltage(voltageSignalDescriptor);
        excitationOfThisWinding.set_current(currentSignalDescriptor);

        json result;
        to_json(result, excitationOfThisWinding);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_reflected_primary(std::string secondaryExcitationString, double turnRatio){
    try {
        OperatingPointExcitation secondaryExcitation(json::parse(secondaryExcitationString));

        OperatingPointExcitation excitationOfThisWinding(secondaryExcitation);
        auto voltageSignalDescriptor = OpenMagnetics::Inputs::reflect_waveform(secondaryExcitation.get_voltage().value(), turnRatio);
        auto currentSignalDescriptor = OpenMagnetics::Inputs::reflect_waveform(secondaryExcitation.get_current().value(), 1.0 / turnRatio);

        auto voltageSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
        voltageSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(voltageSampledWaveform, excitationOfThisWinding.get_frequency()));
        voltageSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));

        auto currentSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
        currentSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
        currentSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));

        excitationOfThisWinding.set_voltage(voltageSignalDescriptor);
        excitationOfThisWinding.set_current(currentSignalDescriptor);

        json result;
        to_json(result, excitationOfThisWinding);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

double calculate_instantaneous_power(std::string excitationString){
    OperatingPointExcitation excitation(json::parse(excitationString));

    if (!excitation.get_current().value().get_processed().value().get_rms().value()) {
        auto current = excitation.get_current().value();
        auto processed = OpenMagnetics::Inputs::calculate_processed_data(current.get_harmonics().value(), current.get_waveform().value(), true);
        current.set_processed(processed);
        excitation.set_current(current);
    }
    if (!excitation.get_voltage().value().get_processed().value().get_rms().value()) {
        auto voltage = excitation.get_voltage().value();
        auto processed = OpenMagnetics::Inputs::calculate_processed_data(voltage.get_harmonics().value(), voltage.get_waveform().value(), true);
        voltage.set_processed(processed);
        excitation.set_voltage(voltage);
    }

    auto instantaneousPower = OpenMagnetics::Inputs::calculate_instantaneous_power(excitation);

    return instantaneousPower;
}

double calculate_rms_power(std::string excitationString){
    try {
        OperatingPointExcitation excitation(json::parse(excitationString));

        if (!excitation.get_voltage() || !excitation.get_current()) {
            return 0.0;
        }

        auto voltageSignalDescriptor = excitation.get_voltage().value();
        auto currentSignalDescriptor = excitation.get_current().value();

        if (!voltageSignalDescriptor.get_waveform() || !currentSignalDescriptor.get_waveform()) {
            return 0.0;
        }

        if (!voltageSignalDescriptor.get_processed() || !voltageSignalDescriptor.get_processed()->get_rms()) {
            auto voltageSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitation.get_frequency());
            voltageSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(voltageSampledWaveform, excitation.get_frequency()));
            voltageSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));
        }

        if (!currentSignalDescriptor.get_processed() || !currentSignalDescriptor.get_processed()->get_rms()) {
            auto currentSampledWaveform = OpenMagnetics::Inputs::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitation.get_frequency());
            currentSignalDescriptor.set_harmonics(OpenMagnetics::Inputs::calculate_harmonics_data(currentSampledWaveform, excitation.get_frequency()));
            currentSignalDescriptor.set_processed(OpenMagnetics::Inputs::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));
        }

        if (!currentSignalDescriptor.get_processed()->get_rms() || !voltageSignalDescriptor.get_processed()->get_rms()) {
            return 0.0;
        }

        double rmsPower = currentSignalDescriptor.get_processed().value().get_rms().value() * voltageSignalDescriptor.get_processed().value().get_rms().value();

        return rmsPower;
    }
    catch (const std::exception &exc) {
        return 0.0;
    }
}

double resolve_dimension_with_tolerance(std::string dimensionWithToleranceString) {
    DimensionWithTolerance dimensionWithTolerance(json::parse(dimensionWithToleranceString));
    return OpenMagnetics::resolve_dimensional_values(dimensionWithTolerance);
}

std::string calculate_basic_processed_data(std::string waveformString) {
    try {
        Waveform waveform(json::parse(waveformString));
        auto processed = OpenMagnetics::Inputs::calculate_basic_processed_data(waveform);
        json result;
        to_json(result, processed);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string create_waveform(std::string processedString, double frequency) {
    try {
        Processed processed(json::parse(processedString));
        auto waveform = OpenMagnetics::Inputs::create_waveform(processed, frequency);
        json result;
        to_json(result, waveform);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string scale_waveform_time_to_frequency(std::string waveformString, double newFrequency) {
    try {
        Waveform waveform(json::parse(waveformString));
        auto scaledWaveform = OpenMagnetics::Inputs::scale_time_to_frequency(waveform, newFrequency);
        json result;
        to_json(result, scaledWaveform);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string scale_excitation_time_to_frequency(std::string excitationString, double newFrequency) {
    try {
        OperatingPointExcitation excitation(json::parse(excitationString));
        OpenMagnetics::Inputs::scale_time_to_frequency(excitation, newFrequency, false, true);
        json result;
        to_json(result, excitation);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_insulation(std::string inputsString){
    auto standard = OpenMagnetics::InsulationCoordinator();
    OpenMagnetics::Inputs inputs(json::parse(inputsString), false);

    json result;
    try {
        result["creepageDistance"] = standard.calculate_creepage_distance(inputs);
        result["clearance"] = standard.calculate_clearance(inputs);
        result["withstandVoltage"] = standard.calculate_withstand_voltage(inputs);
        result["distanceThroughInsulation"] = standard.calculate_distance_through_insulation(inputs);
        result["errorMessage"] = "";
    }
    catch(const std::runtime_error& re)
    {
        result["errorMessage"] = "Exception: " + std::string{re.what()};
    }
    catch(const std::exception& ex)
    {
        result["errorMessage"] = "Exception: " + std::string{ex.what()};
    }
    catch(...)
    {
        result["errorMessage"] = "Unknown failure occurred. Possible memory corruption";
    }
        
        return result.dump(4);
}

std::string extract_operating_point(std::string fileString, size_t numberWindings, double frequency, double desiredMagnetizingInductance, std::string mapColumnNamesString){
    try {
        std::vector<std::map<std::string, std::string>> mapColumnNames = json::parse(mapColumnNamesString).get<std::vector<std::map<std::string, std::string>>>();
        auto reader = OpenMagnetics::CircuitSimulationReader(fileString, true);
        auto operatingPoint = reader.extract_operating_point(numberWindings, frequency, mapColumnNames);
        operatingPoint = OpenMagnetics::Inputs::process_operating_point(operatingPoint, desiredMagnetizingInductance);
        json result;
        to_json(result, operatingPoint);
        return result.dump(4);
    }
    catch(...)
    {
        return "Error processing waveforms, please check column names and frequency";
    }
}

std::string extract_map_column_names(std::string fileString, size_t numberWindings, double frequency){
    try {
        auto reader = OpenMagnetics::CircuitSimulationReader(fileString, true);
        auto columnNames = reader.extract_map_column_names(numberWindings, frequency);

        json result = json::array();
        for (auto& columnName : columnNames) {
            json aux;
            for (auto& [signal, name] : columnName) {
                aux[signal] = name;
            }
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string extract_column_names(std::string fileString){
    try {
        auto reader = OpenMagnetics::CircuitSimulationReader(fileString, true);
        auto columnNames = reader.extract_column_names();

        json result = json::array();
        for (auto& columnName : columnNames) {
            result.push_back(columnName);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<int> calculate_number_turns(int numberTurnsPrimary, std::string designRequirementsString){
    DesignRequirements designRequirements(json::parse(designRequirementsString));

    OpenMagnetics::NumberTurns numberTurns(numberTurnsPrimary, designRequirements);
    auto numberTurnsCombination = numberTurns.get_next_number_turns_combination();

    std::vector<int> numberTurnsResult;
    for (auto turns : numberTurnsCombination) {
        numberTurnsResult.push_back(static_cast<std::make_signed<int>::type>(turns));
    }
    return numberTurnsResult;
}

double calculate_dc_resistance_per_meter(std::string wireString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    auto dcResistancePerMeter = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_meter(wire, temperature);
    return dcResistancePerMeter;
}

std::vector<double> calculate_dc_resistance_per_winding(std::string coilString, double temperature){
    OpenMagnetics::Coil coil(json::parse(coilString), false);
    auto dcResistancePerWinding = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_winding(coil, temperature);
    return dcResistancePerWinding;
}

double calculate_dc_losses_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    return dcLossesPerMeter;
}

double calculate_skin_ac_factor(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    auto skinAcFactor = (skinLossesPerMeter + dcLossesPerMeter) / dcLossesPerMeter;
    return skinAcFactor;
}

double calculate_skin_ac_losses_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    SignalDescriptor current(json::parse(currentString));
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    return skinLossesPerMeter;
}

double calculate_skin_ac_resistance_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    auto skinAcFactor = (skinLossesPerMeter + dcLossesPerMeter) / dcLossesPerMeter;
    auto dcResistancePerMeter = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_meter(wire, temperature);

    return dcResistancePerMeter * skinAcFactor;
}

double calculate_effective_current_density(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::Wire wire(json::parse(wireString));
    SignalDescriptor current(json::parse(currentString));
    auto effectiveCurrentDensity = wire.calculate_effective_current_density(current, temperature);

    return effectiveCurrentDensity;
}

double calculate_effective_skin_depth(std::string material, std::string currentString, double temperature){
    try {
        SignalDescriptor current(json::parse(currentString));

        if (!current.get_processed()->get_effective_frequency()) {
            throw std::runtime_error("Current processed is missing field effective frequency");
        }
        auto currentEffectiveFrequency = current.get_processed()->get_effective_frequency().value();
        double effectiveSkinDepth = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_depth(material, currentEffectiveFrequency, temperature);
        return effectiveSkinDepth;
    }
    catch(const std::exception& ex)
    {
        return -1;
    }
}

std::vector<std::string> get_available_winding_orientations(){
    std::vector<std::string> orientations;

    for (size_t index = 0; index < magic_enum::enum_count<WindingOrientation>(); ++index) {
        auto orientation = static_cast<WindingOrientation>(index);
        orientations.push_back(OpenMagnetics::to_string(orientation));
    }
    return orientations;
}

std::vector<std::string> get_available_coil_alignments(){
    std::vector<std::string> coilAlignments;

    for (size_t index = 0; index < magic_enum::enum_count<CoilAlignment>(); ++index) {
        auto coilAlignment = static_cast<CoilAlignment>(index);
        coilAlignments.push_back(OpenMagnetics::to_string(coilAlignment));
    }
    return coilAlignments;
}

bool check_requirement(std::string requirementString, double value){
    try {
        DimensionWithTolerance requirement(json::parse(requirementString));
        bool result = OpenMagnetics::check_requirement(requirement, value);
        return result;
    }
    catch(const std::exception& ex)
    {
        return false;
    }
}

void process_coil_configuration(OpenMagnetics::Coil& coil, json configuration, std::optional<size_t> repetitions = std::nullopt, std::optional<std::vector<double>> proportionPerWinding = std::nullopt, std::optional<std::vector<size_t>> pattern = std::nullopt) {
    if (repetitions && proportionPerWinding && pattern) {
        if (configuration["_layersOrientation"].is_object()) {
            auto layersOrientationPerSection = std::map<std::string, WindingOrientation>(configuration["_layersOrientation"]);
            for (auto [sectionName, layerOrientation] : layersOrientationPerSection) {
                coil.set_layers_orientation(layerOrientation, sectionName);
            }
        }
        else if (configuration["_layersOrientation"].is_array()) {
            coil.wind_by_sections(proportionPerWinding.value(), pattern.value(), repetitions.value());
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto layersOrientationPerSection = std::vector<WindingOrientation>(configuration["_layersOrientation"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < layersOrientationPerSection.size()) {
                        coil.set_layers_orientation(layersOrientationPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            WindingOrientation layerOrientation(configuration["_layersOrientation"]);
            coil.set_layers_orientation(layerOrientation);

        }
        if (configuration["_turnsAlignment"].is_object()) {
            auto turnsAlignmentPerSection = std::map<std::string, CoilAlignment>(configuration["_turnsAlignment"]);
            for (auto [sectionName, turnsAlignment] : turnsAlignmentPerSection) {
                coil.set_turns_alignment(turnsAlignment, sectionName);
            }
        }
        else if (configuration["_turnsAlignment"].is_array()) {
            coil.wind_by_sections(proportionPerWinding.value(), pattern.value(), repetitions.value());
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto turnsAlignmentPerSection = std::vector<CoilAlignment>(configuration["_turnsAlignment"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < turnsAlignmentPerSection.size()) {
                        coil.set_turns_alignment(turnsAlignmentPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            CoilAlignment turnsAlignment(configuration["_turnsAlignment"]);
            coil.set_turns_alignment(turnsAlignment);
        }
    }
    else {
        if (configuration.contains("_layersOrientation")) {
            coil.set_layers_orientation(configuration["_layersOrientation"]);
        }
        if (configuration.contains("_turnsAlignment")) {
            coil.set_turns_alignment(configuration["_turnsAlignment"]);
        }
    }

    if (configuration.contains("_interleavingLevel")) {
        coil.set_interleaving_level(configuration["_interleavingLevel"]);
    }
    if (configuration.contains("_windingOrientation")) {
        coil.set_winding_orientation(configuration["_windingOrientation"]);
    }
    if (configuration.contains("_sectionAlignment")) {
        coil.set_section_alignment(configuration["_sectionAlignment"]);
    }
    if (configuration.contains("_sectionAlignment")) {
        coil.set_section_alignment(configuration["_sectionAlignment"]);
    }

    if (configuration.contains("_interlayerInsulationThickness")) {
        coil.set_interlayer_insulation(configuration["_interlayerInsulationThickness"], std::nullopt, std::nullopt, false);
    }
    if (configuration.contains("_intersectionInsulationThickness")) {
        coil.set_intersection_insulation(configuration["_intersectionInsulationThickness"], 1, std::nullopt, std::nullopt, false);
    }

}

std::string wind(std::string coilString, size_t repetitions, std::string proportionPerWindingString, std::string patternString, std::string marginPairsString) {
    try {
        auto coilJson = json::parse(coilString);
        auto marginPairs = std::vector<std::vector<double>>(json::parse(marginPairsString));
        
        OpenMagnetics::Settings::GetInstance().set_coil_wind_even_if_not_fit(true);
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Settings::GetInstance().set_coil_include_additional_coordinates(true);
        
        std::vector<double> proportionPerWinding = json::parse(proportionPerWindingString);
        std::vector<size_t> pattern = json::parse(patternString);
        auto winding = std::vector<OpenMagnetics::Winding>(coilJson["functionalDescription"]);
        OpenMagnetics::Coil coil;
        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(winding);
        coil.preload_margins(marginPairs);

        process_coil_configuration(coil, coilJson, repetitions, proportionPerWinding, pattern);

        if (proportionPerWinding.size() == winding.size()) {
            if (pattern.size() > 0 && repetitions > 0) {
                coil.wind(proportionPerWinding, pattern, repetitions);
            }
            else if (repetitions > 0) {
                coil.wind(repetitions);
            }
            else {
                coil.wind();
            }
        }
        else {
            if (pattern.size() > 0 && repetitions > 0) {
                coil.wind(pattern, repetitions);
            }
            else if (repetitions > 0) {
                coil.wind(repetitions);
            }
            else {
                coil.wind();
            }
        }

        if (!coil.get_turns_description()) {
            throw std::runtime_error("Turns not created");
        }

        // Explicitly call delimit_and_compact to ensure toroidal additional turns are compacted
        coil.delimit_and_compact();

        json result;
        to_json(result, coil);
        
        // Debug: Check if additional_coordinates are in the output
        size_t turnsWithAdditionalCoords = 0;
        if (result.contains("turnsDescription") && result["turnsDescription"].is_array()) {
            for (const auto& turn : result["turnsDescription"]) {
                if (turn.contains("additionalCoordinates") && !turn["additionalCoordinates"].is_null()) {
                    turnsWithAdditionalCoords++;
                }
            }
        }
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << coilString << std::endl;
        std::cerr << repetitions << std::endl;
        std::cerr << proportionPerWindingString << std::endl;
        std::cerr << patternString << std::endl;
        std::cerr << marginPairsString << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string wind_planar(std::string coilString, std::string stackUpString, double borderToWireDistance, std::string wireToWireDistanceString, std::string insulationThicknessString, double coreToLayerDistance) {
    try {
        OpenMagnetics::Settings::GetInstance().set_coil_wind_even_if_not_fit(true);
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Settings::GetInstance().set_coil_include_additional_coordinates(true);
        auto coilJson = json::parse(coilString);
        auto coil = OpenMagnetics::Coil(coilJson, false);
        std::vector<size_t> stackUp = json::parse(stackUpString);
        std::map<std::pair<size_t, size_t>, double> insulationThickness = json::parse(insulationThicknessString).get<std::map<std::pair<size_t, size_t>, double>>();
        std::map<size_t, double> wireToWireDistance = json::parse(wireToWireDistanceString).get<std::map<size_t, double>>();

        coil.set_strict(false);
        coil.wind_planar(stackUp, borderToWireDistance, wireToWireDistance, insulationThickness, coreToLayerDistance);

        if (!coil.get_turns_description()) {
            throw std::runtime_error("Turns not created");
        }

        // Explicitly call delimit_and_compact to ensure proper compacting
        coil.delimit_and_compact();

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << coilString << std::endl;
        std::cerr << stackUpString << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string wind_by_sections(std::string coilString, size_t repetitions, std::string proportionPerWindingString, std::string patternString) {
    try {
        auto coilJson = json::parse(coilString);

        std::vector<double> proportionPerWinding = json::parse(proportionPerWindingString);
        std::vector<size_t> pattern = json::parse(patternString);
        auto winding = std::vector<OpenMagnetics::Winding>(coilJson["functionalDescription"]);
        OpenMagnetics::Coil coil;

        process_coil_configuration(coil, coilString, repetitions, proportionPerWinding, pattern);

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(winding);
        if (proportionPerWinding.size() == winding.size()) {
            if (pattern.size() > 0 && repetitions > 0) {
                coil.wind_by_sections(proportionPerWinding, pattern, repetitions);
            }
            else if (repetitions > 0) {
                coil.wind_by_sections(repetitions);
            }
            else {
                coil.wind_by_sections();
            }
        }
        else {
            if (pattern.size() > 0 && repetitions > 0) {
                coil.wind_by_sections(pattern, repetitions);
            }
            else if (repetitions > 0) {
                coil.wind_by_sections(repetitions);
            }
            else {
                coil.wind_by_sections();
            }
        }

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string wind_by_layers(std::string coilString) {
    try {
        auto coilJson = json::parse(coilString);

        auto winding = std::vector<OpenMagnetics::Winding>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        OpenMagnetics::Coil coil;

        process_coil_configuration(coil, coilString);

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(winding);
        coil.set_sections_description(coilSectionsDescription);
        coil.wind_by_layers();

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string wind_by_turns(std::string coilString) {
    try {
        auto coilJson = json::parse(coilString);

        auto winding = std::vector<OpenMagnetics::Winding>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<Layer>(coilJson["layersDescription"]);
        OpenMagnetics::Coil coil;

        process_coil_configuration(coil, coilString);

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(winding);
        coil.set_sections_description(coilSectionsDescription);
        coil.set_layers_description(coilLayersDescription);
        coil.wind_by_turns();

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string delimit_and_compact(std::string coilString) {
    try {
        auto coilJson = json::parse(coilString);

        auto winding = std::vector<OpenMagnetics::Winding>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<Layer>(coilJson["layersDescription"]);
        auto coilTurnsDescription = std::vector<Turn>(coilJson["turnsDescription"]);
        OpenMagnetics::Coil coil;

        process_coil_configuration(coil, coilString);

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(winding);
        coil.set_sections_description(coilSectionsDescription);
        coil.set_layers_description(coilLayersDescription);
        coil.set_turns_description(coilTurnsDescription);
        
        // Preserve groupsDescription if it exists
        if (coilJson.contains("groupsDescription") && !coilJson["groupsDescription"].is_null()) {
            auto groupsDescription = std::vector<OpenMagnetics::Group>(coilJson["groupsDescription"]);
            coil.set_groups_description(groupsDescription);
        }
        
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Settings::GetInstance().set_coil_include_additional_coordinates(true);
        
        coil.delimit_and_compact();

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_layers_by_winding_index(std::string coilString, int windingIndex){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);

        json result = json::array();
        for (auto& layer : coil.get_layers_by_winding_index(windingIndex)) {
            json aux;
            to_json(aux, layer);
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_layers_by_section(std::string coilString, std::string sectionName){
    try {
        json result = json::array();
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        for (auto& layer : coil.get_layers_by_section(sectionName)) {
            json aux;
            to_json(aux, layer);
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_sections_description_conduction(std::string coilString){
    try {
        json result = json::array();
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        for (auto& section : coil.get_sections_description_conduction()) {
            json aux;
            to_json(aux, section);
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

bool are_sections_and_layers_fitting(std::string coilString) {
    try {
        json result = json::array();
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        return coil.are_sections_and_layers_fitting();
    }
    catch (const std::exception &exc) {
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
        return false;
    }
}

std::string add_margin_to_section_by_index(std::string coilString, int sectionIndex, double top_or_left_margin, double bottom_or_right_margin) {
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        coil.add_margin_to_section_by_index(sectionIndex, {top_or_left_margin, bottom_or_right_margin});

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate(std::string inputsString,
                     std::string magneticString,
                     std::string modelsData){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OpenMagnetics::Inputs inputs(json::parse(inputsString));

        auto defaults = OpenMagnetics::Defaults();

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

        for (const auto& [key, value] : models) {
        }

        auto reluctanceModelName = OpenMagnetics::defaults.reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }
        auto coreLossesModelName = OpenMagnetics::defaults.coreLossesModelDefault;
        if (models.find("coreLosses") != models.end()) {
            OpenMagnetics::from_json(models["coreLosses"], coreLossesModelName);
        }
        auto coreTemperatureModelName = OpenMagnetics::defaults.coreTemperatureModelDefault;
        if (models.find("coreTemperature") != models.end()) {
            OpenMagnetics::from_json(models["coreTemperature"], coreTemperatureModelName);
        }


        OpenMagnetics::MagneticSimulator magneticSimulator;

        magneticSimulator.set_core_losses_model_name(coreLossesModelName);
        magneticSimulator.set_core_temperature_model_name(coreTemperatureModelName);
        magneticSimulator.set_reluctance_model_name(reluctanceModelName);
        auto mas = magneticSimulator.simulate(inputs, magnetic);

        if (mas.get_outputs()[0].get_winding_losses()) {
        }

        json result;
        to_json(result, mas);

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


bool check_if_fits(std::string bobbinString, double dimension, bool isHorizontalOrRadial) {
    try {
        OpenMagnetics::Bobbin bobbin(json::parse(bobbinString));
        return bobbin.check_if_fits(dimension, isHorizontalOrRadial);
    }
    catch (const std::exception &exc) {
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
        return false;
    }
}

std::string export_magnetic_as_subcircuit(std::string magneticString, double temperature, std::string simulatorString, std::string jsimba) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        OpenMagnetics::CircuitSimulatorExporterModels simulator;
        from_json(simulatorString, simulator);

        switch(simulator) {
            case OpenMagnetics::CircuitSimulatorExporterModels::SIMBA:
                {
                    std::string subcircuit;
                    if (jsimba != "") {
                        subcircuit = OpenMagnetics::CircuitSimulatorExporter(simulator).export_magnetic_as_subcircuit(magnetic, OpenMagnetics::Defaults().measurementFrequency, temperature, std::nullopt, jsimba);
                    }
                    else {
                        subcircuit = OpenMagnetics::CircuitSimulatorExporter(simulator).export_magnetic_as_subcircuit(magnetic, OpenMagnetics::Defaults().measurementFrequency, temperature);
                    }
                    return subcircuit;
                    break;
                }
            case OpenMagnetics::CircuitSimulatorExporterModels::LTSPICE:
                return OpenMagnetics::CircuitSimulatorExporter(simulator).export_magnetic_as_subcircuit(magnetic, OpenMagnetics::Defaults().measurementFrequency, temperature);
                break;
            case OpenMagnetics::CircuitSimulatorExporterModels::NGSPICE:
                return OpenMagnetics::CircuitSimulatorExporter(simulator).export_magnetic_as_subcircuit(magnetic, OpenMagnetics::Defaults().measurementFrequency, temperature);
                break;
        }


    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


std::string export_magnetic_as_symbol(std::string magneticString, std::string simulatorString, std::string jsimba) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        OpenMagnetics::CircuitSimulatorExporterModels simulator;
        from_json(simulatorString, simulator);

        switch(simulator) {
            case OpenMagnetics::CircuitSimulatorExporterModels::SIMBA:
                break;
            case OpenMagnetics::CircuitSimulatorExporterModels::LTSPICE:
                return OpenMagnetics::CircuitSimulatorExporter(simulator).export_magnetic_as_symbol(magnetic);
                break;
            case OpenMagnetics::CircuitSimulatorExporterModels::NGSPICE:
                break;
        }

        return "";

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


void calculate_ac_resistance_coefficients_per_winding(std::string magneticString, double temperature) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        auto coefficientsPerWinding = OpenMagnetics::CircuitSimulatorExporter(OpenMagnetics::CircuitSimulatorExporterModels::LTSPICE).calculate_ac_resistance_coefficients_per_winding(magnetic, temperature);
        for (auto coefficients : coefficientsPerWinding) {
            for (auto coefficient : coefficients) {
                std::cout << "coefficient: " << coefficient << std::endl;
            }
        }

    }
    catch (const std::exception &exc) {
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
    }
}


std::string sweep_impedance_over_frequency(std::string magneticString, double start, double stop, size_t numberElements, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        auto impedanceOverFrequency = OpenMagnetics::Sweeper::sweep_impedance_over_frequency(magnetic, start, stop, numberElements, mode, title);

        json result;
        to_json(result, impedanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


std::string sweep_q_factor_over_frequency(std::string magneticString, double start, double stop, size_t numberElements, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        auto impedanceOverFrequency = OpenMagnetics::Sweeper::sweep_q_factor_over_frequency(magnetic, start, stop, numberElements, mode, title);

        json result;
        to_json(result, impedanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


std::string sweep_winding_resistance_over_frequency(std::string magneticString, double start, double stop, size_t numberElements, size_t windingIndex, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto resistanceOverFrequency = OpenMagnetics::Sweeper::sweep_winding_resistance_over_frequency(magnetic, start, stop, numberElements, windingIndex, temperature, mode, title);

        json result;
        to_json(result, resistanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_resistance_over_frequency(std::string magneticString, double start, double stop, size_t numberElements, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto resistanceOverFrequency = OpenMagnetics::Sweeper::sweep_resistance_over_frequency(magnetic, start, stop, numberElements, temperature, mode, title);

        json result;
        to_json(result, resistanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_magnetizing_inductance_over_frequency(std::string magneticString, double start, double stop, size_t numberElements, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto magnetizingInductanceOverFrequency = OpenMagnetics::Sweeper::sweep_magnetizing_inductance_over_frequency(magnetic, start, stop, numberElements, temperature, mode, title);

        json result;
        to_json(result, magnetizingInductanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_magnetizing_inductance_over_temperature(std::string magneticString, double start, double stop, size_t numberElements, double frequency, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto magnetizingInductanceOverTemperature = OpenMagnetics::Sweeper::sweep_magnetizing_inductance_over_temperature(magnetic, start, stop, numberElements, frequency, mode, title);

        json result;
        to_json(result, magnetizingInductanceOverTemperature);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_magnetizing_inductance_over_dc_bias(std::string magneticString, double start, double stop, size_t numberElements, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto magnetizingInductanceOverDcBias = OpenMagnetics::Sweeper::sweep_magnetizing_inductance_over_dc_bias(magnetic, start, stop, numberElements, temperature, mode, title);

        json result;
        to_json(result, magnetizingInductanceOverDcBias);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_core_losses_over_frequency(std::string magneticString, std::string operatingPointString, double start, double stop, size_t numberElements, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        auto resistanceOverFrequency = OpenMagnetics::Sweeper::sweep_core_losses_over_frequency(magnetic, operatingPoint, start, stop, numberElements, temperature, mode, title);

        json result;
        to_json(result, resistanceOverFrequency);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string sweep_winding_losses_over_frequency(std::string magneticString, std::string operatingPointString, double start, double stop, size_t numberElements, double temperature, std::string mode, std::string title) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        auto sweep = OpenMagnetics::Sweeper::sweep_winding_losses_over_frequency(magnetic, operatingPoint, start, stop, numberElements, temperature, mode, title);

        json result;
        to_json(result, sweep);

        return result.dump(4);

    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

size_t load_core_materials(std::string fileToLoad){
    try {
        if (fileToLoad != "") {
            OpenMagnetics::load_core_materials(fileToLoad);
        }
        else {
            OpenMagnetics::load_core_materials();
        }

        return OpenMagnetics::coreMaterialDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return -1;
    }
}

size_t load_core_shapes(std::string fileToLoad){
    try {
        if (fileToLoad != "") {
            OpenMagnetics::load_core_shapes(true, fileToLoad);
        }
        else {
            OpenMagnetics::load_core_shapes();
        }
        return OpenMagnetics::coreShapeDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return -1;
    }
}

size_t load_wires(std::string fileToLoad){
    try {
        if (fileToLoad != "") {
            OpenMagnetics::load_wires(fileToLoad);
        }
        else {
            OpenMagnetics::load_wires();
        }
        return OpenMagnetics::wireDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return -1;
    }
}

size_t load_cores(std::string fileToLoad, bool includeToroids, bool useOnlyCoresInStock){
    try {
        if (fileToLoad != "") {
            OpenMagnetics::load_cores(fileToLoad);
        }
        else {
            OpenMagnetics::Settings::GetInstance().set_use_toroidal_cores(includeToroids);
            OpenMagnetics::Settings::GetInstance().set_use_only_cores_in_stock(useOnlyCoresInStock);
            OpenMagnetics::load_cores();
        }
        return OpenMagnetics::coreDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return -1;
    }
}

void clear_loaded_cores(){
    OpenMagnetics::clear_loaded_cores();
}

void clear_databases(){
    OpenMagnetics::clear_databases();
}

bool is_core_material_database_empty(){
    return OpenMagnetics::coreMaterialDatabase.size() == 0;
}

bool is_core_shape_database_empty(){
    return OpenMagnetics::coreShapeDatabase.size() == 0;
}

bool is_wire_database_empty(){
    return OpenMagnetics::wireDatabase.size() == 0;
}

std::vector<double> get_maximum_dimensions(std::string magneticString){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        return magnetic.get_maximum_dimensions();
    }
    catch (const std::exception &exc) {
        std::cerr << "Exception: " + std::string{exc.what()} << std::endl;
        return {};
    }
}

std::string calculate_advised_cores(std::string inputsString, std::string weightsString, int maximumNumberResults, std::string coreModeString){
    try {
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);

        std::cout << "=== DEBUG calculate_advised_cores ===" << std::endl;
        std::cout << "weightsString: " << weightsString << std::endl;
        std::cout << "coreModeString: " << coreModeString << std::endl;

        OpenMagnetics::Inputs inputs(json::parse(inputsString));
        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        from_json(coreModeString, coreMode);
        std::map<std::string, double> weightsKeysString = json::parse(weightsString);
        std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;

        bool filterMode = bool(inputs.get_design_requirements().get_minimum_impedance());

        if (filterMode) {
            OpenMagnetics::Settings::GetInstance().set_use_toroidal_cores(true);
            OpenMagnetics::Settings::GetInstance().set_use_only_cores_in_stock(false);
            OpenMagnetics::Settings::GetInstance().set_use_concentric_cores(false);
        }

        double externalSum = 0;
        for (auto const& pair : weightsKeysString) {
            externalSum += pair.second;
        }

        std::cout << "Parsed weights:" << std::endl;
        for (auto const& [filterName, weight] : weightsKeysString) {
            OpenMagnetics::CoreAdviser::CoreAdviserFilters filter;
            OpenMagnetics::from_json(filterName, filter);
            weights[filter] = weight / externalSum;
            std::cout << "  " << filterName << " -> " << (weight / externalSum) << std::endl;
        }

        OpenMagnetics::CoreAdviser coreAdviser;
        coreAdviser.set_mode(coreMode);
        std::cout << "[DEBUG] CoreAdviser mode set to: " << (int)coreMode << " (0=STANDARD, 1=AVAILABLE, 2=CUSTOM)" << std::endl;
        auto masMagnetics = coreAdviser.get_advised_core(inputs, weights, maximumNumberResults);
        std::cout << "[DEBUG] Results count: " << masMagnetics.size() << std::endl;
        for (size_t i = 0; i < masMagnetics.size() && i < 5; ++i) {
            auto& magnetic = masMagnetics[i].first.get_magnetic();
            std::string coreName = magnetic.get_core().get_name() ? magnetic.get_core().get_name().value() : "unnamed";
            std::cout << "[DEBUG] Result " << i << ": " << coreName << " - Score: " << masMagnetics[i].second << std::endl;
        }
        auto log = OpenMagnetics::read_log();
        auto scoring = coreAdviser.get_scorings();
        std::map<std::string, std::map<std::string, double>> filteredScoring;

        json results = json();
        results["data"] = json::array();
        for (auto& masMagnetic : masMagnetics) {
            std::string name = masMagnetic.first.get_magnetic().get_manufacturer_info().value().get_reference().value();
            auto mas = masMagnetic.first;
            // Extra outputs
            {
                OpenMagnetics::MagnetizingInductance magnetizingInductanceModel;
                for (size_t operatingPointIndex = 0; operatingPointIndex < inputs.get_operating_points().size(); ++operatingPointIndex) {
                    auto operatingPoint = inputs.get_operating_point(operatingPointIndex);
                    auto magnetizingInductanceOutput = magnetizingInductanceModel.calculate_inductance_from_number_turns_and_gapping(mas.get_magnetic().get_core(), mas.get_magnetic().get_coil(), &operatingPoint);
                    if (mas.get_mutable_outputs()[operatingPointIndex].get_inductance()) {
                        auto magnetizingInductanceOutputEnergy = mas.get_mutable_outputs()[operatingPointIndex].get_inductance()->get_magnetizing_inductance();
                        magnetizingInductanceOutput.set_maximum_magnetic_energy_core(magnetizingInductanceOutputEnergy.get_maximum_magnetic_energy_core());
                        InductanceOutput inductanceOutput = *mas.get_mutable_outputs()[operatingPointIndex].get_inductance();
                        inductanceOutput.set_magnetizing_inductance(magnetizingInductanceOutput);
                        mas.get_mutable_outputs()[operatingPointIndex].set_inductance(inductanceOutput);
                    }
                    masMagnetic.first = mas;
                }
            }

            json result;
            json masJson;
            to_json(masJson, masMagnetic.first);
            result["mas"] = masJson;
            // result["weightedTotalScoring"] = masMagnetic.second;
            result["weightedTotalScoring"] = scoring[name][OpenMagnetics::CoreAdviser::CoreAdviserFilters::COST] + scoring[name][OpenMagnetics::CoreAdviser::CoreAdviserFilters::EFFICIENCY] + scoring[name][OpenMagnetics::CoreAdviser::CoreAdviserFilters::DIMENSIONS];
            result["scoringPerFilter"] = json();

            for (size_t index = 0; index < magic_enum::enum_count<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(); ++index) {
                auto filter = static_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(index);
                auto filterString = OpenMagnetics::to_string(filter);
                result["scoringPerFilter"][filterString] = scoring[name][filter];
            };
            results["data"].push_back(result);
        }
        results["log"] = log;

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << inputsString << std::endl;
        std::cerr << weightsString << std::endl;
        std::cerr << maximumNumberResults << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_sections(std::string masString, std::string patternString, int repetitions){
    try {
        OpenMagnetics::Mas mas(json::parse(masString));
        json patternJson = json::parse(patternString);
        std::vector<size_t> pattern; 
        for (auto& elem : patternJson) {
            pattern.push_back(elem);
        }

        auto bobbin = mas.get_magnetic().get_coil().get_bobbin();
        if (std::holds_alternative<std::string>(bobbin)) {
            auto bobbinString = std::get<std::string>(bobbin);
            if (bobbinString == "Dummy") {
                mas.get_mutable_magnetic().get_mutable_coil().set_bobbin(OpenMagnetics::Bobbin::create_quick_bobbin(mas.get_mutable_magnetic().get_mutable_core()));
            }
        }
        for (size_t windingIndex = 0; windingIndex < mas.get_magnetic().get_coil().get_functional_description().size(); ++windingIndex) {
            mas.get_mutable_magnetic().get_mutable_coil().get_mutable_functional_description()[windingIndex].set_wire("Dummy");
        }

        auto sections = OpenMagnetics::CoilAdviser().get_advised_sections(mas, pattern, repetitions);
        json result = json::array();
        for (auto& section : sections) {
            json aux;
            to_json(aux, section);
            result.push_back(aux);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_coil(std::string masString){
    try {
        // Log the raw input string for debugging
        std::cout << "=== INPUT_JSON_START ===" << std::endl;
        std::cout << masString << std::endl;
        std::cout << "=== INPUT_JSON_END ===" << std::endl;
        
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Mas mas(json::parse(masString));

        for (size_t windingIndex = 0; windingIndex < mas.get_magnetic().get_coil().get_functional_description().size(); ++windingIndex) {
            mas.get_mutable_magnetic().get_mutable_coil().get_mutable_functional_description()[windingIndex].set_wire("Dummy");
        }
        mas.get_mutable_magnetic().get_mutable_coil().set_turns_description(std::nullopt);
        mas.get_mutable_magnetic().get_mutable_coil().set_layers_description(std::nullopt);
        mas.get_mutable_magnetic().get_mutable_coil().set_sections_description(std::nullopt);
        mas.get_mutable_magnetic().get_mutable_coil().set_groups_description(std::nullopt);

        OpenMagnetics::CoilAdviser coilAdviser;
        auto masMagneticsWithCoil = coilAdviser.get_advised_coil(mas, 1);

        if (masMagneticsWithCoil.size() > 0) {
            json result = json();
            to_json(result, masMagneticsWithCoil[0]);
        std::cout << "=== OUTPUT_JSON_START ===" << std::endl;
        std::cout << result << std::endl;
        std::cout << "=== OUTPUT_JSON_END ===" << std::endl;
            return result.dump(4);
        }
        else{
            std::cerr << masString << std::endl;
            return "Exception: No coil found";
        }
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_wires(std::string windingString,
                                    std::string sectionString,
                                    std::string currentString,
                                    std::string solidInsulationRequirementsString,
                                    double temperature,
                                    uint8_t numberSections,
                                    size_t maximumNumberResults,
                                    bool usePlanarWires){
    try {
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Winding winding(json::parse(windingString));
        OpenMagnetics::WireSolidInsulationRequirements wireSolidInsulationRequirements(json::parse(solidInsulationRequirementsString));
        Section section(json::parse(sectionString));
        SignalDescriptor current(json::parse(currentString));

        OpenMagnetics::WireAdviser wireAdviser;
        wireAdviser.set_wire_solid_insulation_requirements(wireSolidInsulationRequirements);
        std::vector<std::pair<OpenMagnetics::Winding, double>> windingsWithScoring;
        
        if (usePlanarWires) {
            windingsWithScoring = wireAdviser.get_advised_planar_wire(winding, section, current, temperature, numberSections, maximumNumberResults);
        }
        else {
            windingsWithScoring = wireAdviser.get_advised_wire(winding, section, current, temperature, numberSections, maximumNumberResults);
        }

        json results = json();
        results["data"] = json::array();
        for (auto& [winding, scoring] : windingsWithScoring) {
            json result;
            json windingJson;
            to_json(windingJson, winding);
            result["winding"] = windingJson;
            result["scoring"] = scoring;
            results["data"].push_back(result);
        }

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_solid_insulation_requirements_for_wires(std::string inputsString, std::string patternString, int repetitions) {
    try {
        OpenMagnetics::Inputs inputs(json::parse(inputsString));
        json patternJson = json::parse(patternString);
        std::vector<size_t> pattern; 
        for (auto& elem : patternJson) {
            pattern.push_back(elem);
        }

        auto results = json::array();
        auto solidInsulationRequirementsCombinations = OpenMagnetics::InsulationCoordinator().get_solid_insulation_requirements_for_wires(inputs, pattern, repetitions);
        for (auto solidInsulationRequirementsCombination : solidInsulationRequirementsCombinations) {
            auto aux = json::array();
            for (auto solidInsulationRequirementsForWires : solidInsulationRequirementsCombination) {
                json solidInsulationRequirementsForWiresJson;
                to_json(solidInsulationRequirementsForWiresJson, solidInsulationRequirementsForWires);
                aux.push_back(solidInsulationRequirementsForWiresJson);
            }
            results.push_back(aux);
        }
        return results.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_magnetics(std::string inputsString, std::string weightsString, int maximumNumberResults, std::string coreModeString){
    try {
        std::cerr << "[DEBUG WASM calculate_advised_magnetics] coreModeString: " << coreModeString << std::endl;
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Inputs inputs(json::parse(inputsString));

        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        from_json(coreModeString, coreMode);
        std::cerr << "[DEBUG WASM calculate_advised_magnetics] coreMode enum value set" << std::endl;

        std::map<std::string, double> weightsKeysString = json::parse(weightsString);
        std::map<OpenMagnetics::MagneticFilters, double> weights;

        double externalSum = 0;
        for (auto const& pair : weightsKeysString) {
            externalSum += pair.second;
        }

        for (auto const& [filterName, weight] : weightsKeysString) {
            OpenMagnetics::MagneticFilters filter;
            OpenMagnetics::from_json(filterName, filter);
            weights[filter] = weight / externalSum;
        }

        OpenMagnetics::MagneticAdviser magneticAdviser;
        magneticAdviser.set_core_mode(coreMode);
        auto masMagnetics = magneticAdviser.get_advised_magnetic(inputs, weights, maximumNumberResults);
        // auto log = magneticAdviser.read_log();
        auto scorings = magneticAdviser.get_scorings();

        json results = json();
        results["data"] = json::array();
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            std::string name = masMagnetic.get_magnetic().get_manufacturer_info().value().get_reference().value();

            json result;
            json masJson;
            to_json(masJson, masMagnetic);
            result["mas"] = masJson;
            result["weightedTotalScoring"] = scorings[name][OpenMagnetics::MagneticFilters::COST] + scorings[name][OpenMagnetics::MagneticFilters::LOSSES] + scorings[name][OpenMagnetics::MagneticFilters::DIMENSIONS];
            result["scoringPerFilter"] = json();
            for (size_t index = 0; index < magic_enum::enum_count<OpenMagnetics::MagneticFilters>(); ++index) {
                auto filter = static_cast<OpenMagnetics::MagneticFilters>(index);
                auto filterString = OpenMagnetics::to_string(filter);
                if (scorings[name].count(filter)) {
                    result["scoringPerFilter"][filterString] = scorings[name][filter];
                }
            };
            results["data"].push_back(result);
        }

        sort(results["data"].begin(), results["data"].end(), [](json& b1, json& b2) {
            return b1["weightedTotalScoring"] > b2["weightedTotalScoring"];
        });

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_magnetics_from_catalog(std::string inputsString, std::string catalogString, int maximumNumberResults){
    try {
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Inputs inputs(json::parse(inputsString));
        std::map<OpenMagnetics::MagneticFilters, double> weights;

        std::vector <OpenMagnetics::Magnetic> catalog;

        for (auto& catalogSubstring : OpenMagnetics::split(catalogString, "\n")) {
            OpenMagnetics::Magnetic magnetic(json::parse(catalogSubstring));
            catalog.push_back(magnetic);
        }

        OpenMagnetics::MagneticAdviser magneticAdviser;
        auto masMagnetics = magneticAdviser.get_advised_magnetic(inputs, catalog, maximumNumberResults);

        auto scorings = magneticAdviser.get_scorings();

        json results = json();
        results["data"] = json::array();
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            std::string name = masMagnetic.get_magnetic().get_manufacturer_info().value().get_reference().value();
            json result;
            json masJson;
            to_json(masJson, masMagnetic);
            result["mas"] = masJson;
            result["scoring"] = scoring;
            results["data"].push_back(result);
        }

        sort(results["data"].begin(), results["data"].end(), [](json& b1, json& b2) {
            return b1["scoring"] > b2["scoring"];
        });

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << inputsString << std::endl;
        std::cerr << catalogString << std::endl;
        std::cerr << maximumNumberResults << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_magnetics_from_cache(std::string inputsString, std::string filterFlowString, int maximumNumberResults){
    try {
        OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(true);
        OpenMagnetics::Inputs inputs(json::parse(inputsString));

        std::vector<OpenMagnetics::MagneticFilterOperation> filterFlow;
        json filterFlowJson = json::parse(filterFlowString);
        for (auto filterJson : filterFlowJson) {
            OpenMagnetics::MagneticFilterOperation filter(filterJson);
            filterFlow.push_back(filter);
        }

        if (OpenMagnetics::magneticsCache.size() == 0) {
            return "Exception: No magnetics found in cache";
        }

        OpenMagnetics::MagneticAdviser magneticAdviser;
        auto masMagnetics = magneticAdviser.get_advised_magnetic(inputs, OpenMagnetics::magneticsCache.get(), filterFlow, maximumNumberResults);

        auto scorings = magneticAdviser.get_scorings();

        json results = json();
        results["data"] = json::array();
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            std::string name = masMagnetic.get_magnetic().get_manufacturer_info().value().get_reference().value();
            json result;
            json masJson;
            to_json(masJson, masMagnetic);
            result["mas"] = masJson;
            result["scoring"] = scoring;
            results["data"].push_back(result);
        }

        sort(results["data"].begin(), results["data"].end(), [](json& b1, json& b2) {
            return b1["scoring"] > b2["scoring"];
        });

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << inputsString << std::endl;
        std::cerr << filterFlowString << std::endl;
        std::cerr << maximumNumberResults << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<std::string> get_available_core_filters(){
    std::vector<std::string> filters;
    for (size_t index = 0; index < magic_enum::enum_count<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(); ++index) {
        auto filter = static_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(index);

        filters.push_back(OpenMagnetics::to_string(filter));
    }
    return filters;
}

std::string calculate_leakage_inductance(std::string magneticString, double frequency, size_t sourceIndex){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

        auto leakageInductanceOutput = OpenMagnetics::LeakageInductance().calculate_leakage_inductance_all_windings(magnetic, frequency, sourceIndex);

        json result;
        to_json(result, leakageInductanceOutput);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_inductance_matrix(std::string magneticString, double frequency, std::string modelsData){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::Inductance inductance(reluctanceModelName);
        auto inductanceMatrix = inductance.calculate_inductance_matrix(magnetic, frequency);

        json result;
        to_json(result, inductanceMatrix);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_coupling_coefficient_matrix(std::string magneticString, double frequency, std::string modelsData){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::Inductance inductance(reluctanceModelName);
        
        auto& functionalDescription = magnetic.get_coil().get_functional_description();
        size_t numWindings = functionalDescription.size();
        
        ScalarMatrixAtFrequency result;
        result.set_frequency(frequency);
        
        std::map<std::string, std::map<std::string, DimensionWithTolerance>> magnitude;
        
        // Calculate coupling coefficient matrix
        for (size_t i = 0; i < numWindings; ++i) {
            std::string windingName_i = functionalDescription[i].get_name();
            
            for (size_t j = 0; j < numWindings; ++j) {
                std::string windingName_j = functionalDescription[j].get_name();
                
                double k = inductance.calculate_coupling_coefficient(magnetic, i, j, frequency);
                
                DimensionWithTolerance dimValue;
                dimValue.set_nominal(k);
                magnitude[windingName_i][windingName_j] = dimValue;
            }
        }
        
        result.set_magnitude(magnitude);

        json jsonResult;
        to_json(jsonResult, result);
        return jsonResult.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_leakage_inductance_matrix(std::string magneticString, double frequency, std::string modelsData){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            OpenMagnetics::from_json(models["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::Inductance inductance(reluctanceModelName);
        auto leakageInductanceMatrix = inductance.calculate_leakage_inductance_matrix(magnetic, frequency);

        json result;
        to_json(result, leakageInductanceMatrix);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_stray_capacitance(std::string coilString, std::string operatingPointString, std::string modelsData){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto strayCapacitanceModelName = OpenMagnetics::StrayCapacitanceModels::ALBACH;
        if (models.find("strayCapacitance") != models.end()) {
            OpenMagnetics::from_json(models["strayCapacitance"], strayCapacitanceModelName);
        }

        OpenMagnetics::StrayCapacitance strayCapacitance(strayCapacitanceModelName);
        auto strayCapacitanceOutput = strayCapacitance.calculate_capacitance(coil, operatingPoint);

        json result;
        to_json(result, strayCapacitanceOutput);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_capacitance_matrix(std::string coilString, std::string modelsData){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto strayCapacitanceModelName = OpenMagnetics::StrayCapacitanceModels::ALBACH;
        if (models.find("strayCapacitance") != models.end()) {
            OpenMagnetics::from_json(models["strayCapacitance"], strayCapacitanceModelName);
        }

        OpenMagnetics::StrayCapacitance strayCapacitance(strayCapacitanceModelName);
        auto strayCapacitanceOutput = strayCapacitance.calculate_capacitance(coil);

        json result;
        if (strayCapacitanceOutput.get_capacitance_matrix()) {
            auto capacitanceMatrix = strayCapacitanceOutput.get_capacitance_matrix().value();
            for (const auto& [outerKey, innerMap] : capacitanceMatrix) {
                result[outerKey] = json();
                for (const auto& [innerKey, scalarMatrix] : innerMap) {
                    json matrixJson;
                    to_json(matrixJson, scalarMatrix);
                    result[outerKey][innerKey] = matrixJson;
                }
            }
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_maxwell_capacitance_matrix(std::string coilString, std::string modelsData){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        
        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
        
        auto strayCapacitanceModelName = OpenMagnetics::StrayCapacitanceModels::ALBACH;
        if (models.find("strayCapacitance") != models.end()) {
            OpenMagnetics::from_json(models["strayCapacitance"], strayCapacitanceModelName);
        }

        OpenMagnetics::StrayCapacitance strayCapacitance(strayCapacitanceModelName);
        auto strayCapacitanceOutput = strayCapacitance.calculate_capacitance(coil);

        json result = json::array();
        if (strayCapacitanceOutput.get_maxwell_capacitance_matrix()) {
            auto maxwellMatrix = strayCapacitanceOutput.get_maxwell_capacitance_matrix().value();
            for (const auto& matrix : maxwellMatrix) {
                json matrixJson;
                to_json(matrixJson, matrix);
                result.push_back(matrixJson);
            }
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_capacitance_models_between_windings(double energy, double voltageDrop, double relativeTurnsRatio){
    try {
        auto result = OpenMagnetics::StrayCapacitance::calculate_capacitance_models_between_windings(energy, voltageDrop, relativeTurnsRatio);
        
        json resultJson;
        
        // Serialize SixCapacitorNetworkPerWinding
        resultJson["sixCapacitorNetwork"]["c1"] = result.first.get_c1();
        resultJson["sixCapacitorNetwork"]["c2"] = result.first.get_c2();
        resultJson["sixCapacitorNetwork"]["c3"] = result.first.get_c3();
        resultJson["sixCapacitorNetwork"]["c4"] = result.first.get_c4();
        resultJson["sixCapacitorNetwork"]["c5"] = result.first.get_c5();
        resultJson["sixCapacitorNetwork"]["c6"] = result.first.get_c6();
        
        // Serialize TripoleCapacitancePerWinding
        resultJson["tripoleCapacitance"]["c1"] = result.second.get_c1();
        resultJson["tripoleCapacitance"]["c2"] = result.second.get_c2();
        resultJson["tripoleCapacitance"]["c3"] = result.second.get_c3();
        
        return resultJson.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_available_core_losses_methods(std::string magneticString){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        auto core = magnetic.get_core();
        auto methods = core.get_available_core_losses_methods();
        
        json resultJson;
        resultJson["methods"] = json::array();
        resultJson["hasMaterial"] = true;
        
        // Map VolumetricCoreLossesMethodType to display names in preference order
        std::map<std::string, int> methodPriority = {
            {"steinmetz", 0},
            {"roshen", 1},
            {"lossFactor", 2},
            {"magnetics", 3},
            {"micrometals", 4}
        };
        
        struct MethodInfo {
            std::string key;
            std::string displayName;
            int priority;
        };
        
        std::vector<MethodInfo> methodInfos;
        
        for (const auto& method : methods) {
            json methodJson;
            to_json(methodJson, method);
            std::string methodKey = methodJson.get<std::string>();
            
            std::string displayName;
            if (methodKey == "steinmetz") displayName = "Steinmetz";
            else if (methodKey == "roshen") displayName = "Roshen";
            else if (methodKey == "lossFactor") displayName = "Loss Factor";
            else if (methodKey == "magnetics" || methodKey == "micrometals") displayName = "Proprietary";
            else displayName = methodKey;
            
            int priority = methodPriority.count(methodKey) ? methodPriority[methodKey] : 999;
            methodInfos.push_back({methodKey, displayName, priority});
        }
        
        // Sort by priority
        std::sort(methodInfos.begin(), methodInfos.end(), 
            [](const MethodInfo& a, const MethodInfo& b) {
                return a.priority < b.priority;
            });
        
        for (const auto& info : methodInfos) {
            json methodObj;
            methodObj["key"] = info.key;
            methodObj["displayName"] = info.displayName;
            resultJson["methods"].push_back(methodObj);
        }
        
        return resultJson.dump(4);
    }
    catch (const std::exception &exc) {
        // Return empty methods array if no material or error
        json resultJson;
        resultJson["methods"] = json::array();
        resultJson["hasMaterial"] = false;
        resultJson["error"] = exc.what();
        return resultJson.dump(4);
    }
}

// Helper function to convert snake_case to Title Case
std::string toTitleCase(const std::string& str) {
    std::string result;
    bool capitalizeNext = true;
    for (char c : str) {
        if (c == '_') {
            result += ' ';
            capitalizeNext = true;
        } else if (capitalizeNext) {
            result += std::toupper(c);
            capitalizeNext = false;
        } else {
            result += std::tolower(c);
        }
    }
    return result;
}

// Helper to get all enum values as display names
std::string get_all_magnetic_field_strength_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::MagneticFieldStrengthModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_all_fringing_effect_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::MagneticFieldStrengthFringingEffectModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_all_reluctance_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::ReluctanceModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_all_winding_skin_effect_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::WindingSkinEffectLossesModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_all_winding_proximity_effect_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::WindingProximityEffectLossesModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_all_stray_capacitance_models() {
    try {
        json result = json::array();
        for (const auto& enumValue : magic_enum::enum_values<OpenMagnetics::StrayCapacitanceModels>()) {
            std::string name = std::string(magic_enum::enum_name(enumValue));
            result.push_back(toTitleCase(name));
        }
        return result.dump();
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_resistance_matrix(std::string magneticString, double temperature, double frequency){
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        
        OpenMagnetics::WindingLosses windingLosses;
        auto resistanceMatrix = windingLosses.calculate_resistance_matrix(magnetic, temperature, frequency);

        json result;
        to_json(result, resistanceMatrix);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_flyback_inputs(std::string flybackInputsString){
    try {
        json flybackInputsJson = json::parse(flybackInputsString);

        OpenMagnetics::Flyback flybackInputs(flybackInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (flybackInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = flybackInputsJson["numberOfPeriods"].get<size_t>();
        }
        flybackInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = flybackInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_flyback_inputs(std::string flybackInputsString){
    try {
        json flybackInputsJson = json::parse(flybackInputsString);

        OpenMagnetics::AdvancedFlyback flybackInputs(flybackInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (flybackInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = flybackInputsJson["numberOfPeriods"].get<size_t>();
        }
        flybackInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = flybackInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_flyback_ideal_waveforms(std::string flybackInputsString){
    try {
        json flybackInputsJson = json::parse(flybackInputsString);

        // Detect if this is an AdvancedFlyback (user knows design) or regular Flyback (help with design)
        // AdvancedFlyback has "desiredInductance" field, Flyback has "currentRippleRatio" field
        bool isAdvancedFlyback = flybackInputsJson.contains("desiredInductance");
        
        DesignRequirements designRequirements;
        std::vector<double> turnsRatios;
        double magnetizingInductance;
        
        // Create a unique_ptr to hold either Flyback or AdvancedFlyback
        // AdvancedFlyback inherits from Flyback so we can use the base pointer
        std::unique_ptr<OpenMagnetics::Flyback> flybackPtr;
        
        if (isAdvancedFlyback) {
            // User knows the design they want - use AdvancedFlyback
            // AdvancedFlyback has its own process() method that uses desiredInductance/desiredTurnsRatios/desiredDutyCycle
            auto advancedFlybackPtr = std::make_unique<OpenMagnetics::AdvancedFlyback>(flybackInputsJson);
            
            // Get design parameters directly from the AdvancedFlyback object
            magnetizingInductance = advancedFlybackPtr->get_desired_inductance();
            turnsRatios = advancedFlybackPtr->get_desired_turns_ratios();
            
            // Build designRequirements manually from the desired values
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto turnsRatio : turnsRatios) {
                DimensionWithTolerance turnsRatioWithTolerance;
                turnsRatioWithTolerance.set_nominal(turnsRatio);
                designRequirements.get_mutable_turns_ratios().push_back(turnsRatioWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            std::vector<IsolationSide> isolationSides;
            for (size_t windingIndex = 0; windingIndex < turnsRatios.size() + 1; ++windingIndex) {
                isolationSides.push_back(OpenMagnetics::get_isolation_side_from_index(windingIndex));
            }
            designRequirements.set_isolation_sides(isolationSides);
            designRequirements.set_topology(Topologies::FLYBACK_CONVERTER);
            
            // Move the AdvancedFlyback into the base pointer (polymorphism)
            flybackPtr = std::move(advancedFlybackPtr);
        } else {
            // Help with design - use regular Flyback
            flybackPtr = std::make_unique<OpenMagnetics::Flyback>(flybackInputsJson);
            designRequirements = flybackPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                turnsRatios.push_back(tr.get_nominal().value());
            }
            
            // Extract magnetizing inductance (use minimum if available, otherwise nominal)
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate magnetizing inductance");
            }
        }
        
        // Verify ngspice is available - required for simulation
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (flybackInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = flybackInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (flybackInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = flybackInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        flybackPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
        // Use ngspice-based simulation for accurate waveforms
        auto topologyWaveforms = flybackPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance, numberOfPeriods);
        
        // Also get the operating points for the magnetic data
        auto operatingPoints = flybackPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance, numberOfPeriods);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Note: simulate_flyback_with_magnetic uses ngspice which is not available in WASM.
// This function is kept for PyMKF (native) usage but will throw an error in browser.
std::string simulate_flyback_with_magnetic(std::string flybackInputsString, std::string magneticString){
    try {
        return "Exception: ngspice-based simulation is not available in browser WASM. Use native PyMKF for real magnetic simulation.";
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// SPICE Code Generation Functions - Returns the ngspice netlist for a converter
EMSCRIPTEN_KEEPALIVE std::string generate_flyback_ngspice_circuit(std::string flybackInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json flybackInputsJson = json::parse(flybackInputsString);
        
        bool isAdvancedFlyback = flybackInputsJson.contains("desiredInductance");
        
        std::unique_ptr<OpenMagnetics::Flyback> flybackPtr;
        std::vector<double> turnsRatios;
        double magnetizingInductance;
        
        if (isAdvancedFlyback) {
            auto advancedFlybackPtr = std::make_unique<OpenMagnetics::AdvancedFlyback>(flybackInputsJson);
            magnetizingInductance = advancedFlybackPtr->get_desired_inductance();
            turnsRatios = advancedFlybackPtr->get_desired_turns_ratios();
            flybackPtr = std::move(advancedFlybackPtr);
        } else {
            flybackPtr = std::make_unique<OpenMagnetics::Flyback>(flybackInputsJson);
            auto designRequirements = flybackPtr->process_design_requirements();
            
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                turnsRatios.push_back(tr.get_nominal().value());
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate magnetizing inductance");
            }
        }
        
        std::string netlist = flybackPtr->generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Generic template for converters with turnsRatios and magnetizingInductance
template<typename ConverterType, typename AdvancedConverterType>
std::string generate_converter_ngspice_circuit_helper(std::string inputsString, size_t inputVoltageIndex, size_t operatingPointIndex, const std::string& desiredFieldName) {
    try {
        json inputsJson = json::parse(inputsString);
        
        bool isAdvanced = inputsJson.contains(desiredFieldName);
        
        std::vector<double> turnsRatios;
        double magnetizingInductance;
        
        std::string netlist;
        if (isAdvanced) {
            AdvancedConverterType converter(inputsJson);
            magnetizingInductance = converter.get_desired_inductance();
            turnsRatios = converter.get_desired_turns_ratios();
            netlist = converter.generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        } else {
            ConverterType converter(inputsJson);
            auto designRequirements = converter.process_design_requirements();
            
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                turnsRatios.push_back(tr.get_nominal().value());
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate magnetizing inductance");
            }
            
            netlist = converter.generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        }
        
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Buck SPICE generation (uses inductance directly, not turns ratios)
EMSCRIPTEN_KEEPALIVE std::string generate_buck_ngspice_circuit(std::string buckInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json buckInputsJson = json::parse(buckInputsString);
        
        bool isAdvancedBuck = buckInputsJson.contains("desiredInductance");
        
        double inductance;
        
        std::string netlist;
        if (isAdvancedBuck) {
            OpenMagnetics::AdvancedBuck buck(buckInputsJson);
            inductance = buck.get_desired_inductance();
            netlist = buck.generate_ngspice_circuit(inductance, inputVoltageIndex, operatingPointIndex);
        } else {
            OpenMagnetics::Buck buck(buckInputsJson);
            auto designRequirements = buck.process_design_requirements();
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                inductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                inductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
            
            netlist = buck.generate_ngspice_circuit(inductance, inputVoltageIndex, operatingPointIndex);
        }
        
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Boost SPICE generation (uses inductance directly)
EMSCRIPTEN_KEEPALIVE std::string generate_boost_ngspice_circuit(std::string boostInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json boostInputsJson = json::parse(boostInputsString);
        
        bool isAdvancedBoost = boostInputsJson.contains("desiredInductance");
        
        double inductance;
        
        std::string netlist;
        if (isAdvancedBoost) {
            OpenMagnetics::AdvancedBoost boost(boostInputsJson);
            inductance = boost.get_desired_inductance();
            netlist = boost.generate_ngspice_circuit(inductance, inputVoltageIndex, operatingPointIndex);
        } else {
            OpenMagnetics::Boost boost(boostInputsJson);
            auto designRequirements = boost.process_design_requirements();
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                inductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                inductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
            
            netlist = boost.generate_ngspice_circuit(inductance, inputVoltageIndex, operatingPointIndex);
        }
        
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// PushPull SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_push_pull_ngspice_circuit(std::string pushPullInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::PushPull, OpenMagnetics::AdvancedPushPull>(pushPullInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// Forward SPICE generation (Single Switch)
EMSCRIPTEN_KEEPALIVE std::string generate_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::SingleSwitchForward, OpenMagnetics::AdvancedSingleSwitchForward>(forwardInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// Two Switch Forward SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_two_switch_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::TwoSwitchForward, OpenMagnetics::AdvancedTwoSwitchForward>(forwardInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// Active Clamp Forward SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_active_clamp_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::ActiveClampForward, OpenMagnetics::AdvancedActiveClampForward>(forwardInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// Isolated Buck SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_isolated_buck_ngspice_circuit(std::string isolatedBuckInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::IsolatedBuck, OpenMagnetics::AdvancedIsolatedBuck>(isolatedBuckInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// Isolated Buck Boost SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_isolated_buck_boost_ngspice_circuit(std::string isolatedBuckBoostInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    return generate_converter_ngspice_circuit_helper<OpenMagnetics::IsolatedBuckBoost, OpenMagnetics::AdvancedIsolatedBuckBoost>(isolatedBuckBoostInputsString, inputVoltageIndex, operatingPointIndex, "desiredInductance");
}

// LLC SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_llc_ngspice_circuit(std::string llcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json llcInputsJson = json::parse(llcInputsString);
        OpenMagnetics::Llc llc(llcInputsJson);
        
        // For LLC, we need to extract from functional description or design requirements
        auto designRequirements = llc.process_design_requirements();
        std::vector<double> turnsRatios;
        for (const auto& tr : designRequirements.get_turns_ratios()) {
            turnsRatios.push_back(tr.get_nominal().value());
        }
        
        double magnetizingInductance;
        if (designRequirements.get_magnetizing_inductance().get_minimum()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
        } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
        } else {
            throw std::runtime_error("Unable to calculate magnetizing inductance");
        }
        
        std::string netlist = llc.generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// CLLC SPICE generation - Note: CLLC has a different signature, skipping for now
// EMSCRIPTEN_KEEPALIVE std::string generate_cllc_ngspice_circuit(std::string cllcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
//     // CLLC requires CllcResonantParameters, different from other converters
//     return "Exception: CLLC SPICE generation not yet implemented";
// }

// DAB SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_dab_ngspice_circuit(std::string dabInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json dabInputsJson = json::parse(dabInputsString);
        OpenMagnetics::Dab dab(dabInputsJson);
        
        auto designRequirements = dab.process_design_requirements();
        std::vector<double> turnsRatios;
        for (const auto& tr : designRequirements.get_turns_ratios()) {
            turnsRatios.push_back(tr.get_nominal().value());
        }
        
        double magnetizingInductance;
        if (designRequirements.get_magnetizing_inductance().get_minimum()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
        } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
        } else {
            throw std::runtime_error("Unable to calculate magnetizing inductance");
        }
        
        std::string netlist = dab.generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// Phase Shifted Full Bridge SPICE generation
EMSCRIPTEN_KEEPALIVE std::string generate_psfb_ngspice_circuit(std::string psfbInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json psfbInputsJson = json::parse(psfbInputsString);
        OpenMagnetics::Psfb psfb(psfbInputsJson);
        
        auto designRequirements = psfb.process_design_requirements();
        std::vector<double> turnsRatios;
        for (const auto& tr : designRequirements.get_turns_ratios()) {
            turnsRatios.push_back(tr.get_nominal().value());
        }
        
        double magnetizingInductance;
        if (designRequirements.get_magnetizing_inductance().get_minimum()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
        } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
            magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
        } else {
            throw std::runtime_error("Unable to calculate magnetizing inductance");
        }
        
        std::string netlist = psfb.generate_ngspice_circuit(turnsRatios, magnetizingInductance, inputVoltageIndex, operatingPointIndex);
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_isolated_buck_inputs(std::string isolatedBuckInputsString){
    try {
        json isolatedBuckInputsJson = json::parse(isolatedBuckInputsString);

        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (isolatedBuckInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = isolatedBuckInputsJson["numberOfPeriods"].get<size_t>();
        }

        OpenMagnetics::IsolatedBuck isolatedBuckInputs(isolatedBuckInputsJson);
        auto inputs = isolatedBuckInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_isolated_buck_inputs(std::string isolatedBuckInputsString){
    try {
        json isolatedBuckInputsJson = json::parse(isolatedBuckInputsString);

        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (isolatedBuckInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = isolatedBuckInputsJson["numberOfPeriods"].get<size_t>();
        }

        OpenMagnetics::AdvancedIsolatedBuck isolatedBuckInputs(isolatedBuckInputsJson);
        auto inputs = isolatedBuckInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_isolated_buck_boost_inputs(std::string isolatedBuckBoostInputsString){
    try {
        json isolatedBuckBoostInputsJson = json::parse(isolatedBuckBoostInputsString);

        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (isolatedBuckBoostInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = isolatedBuckBoostInputsJson["numberOfPeriods"].get<size_t>();
        }

        OpenMagnetics::IsolatedBuckBoost isolatedBuckBoostInputs(isolatedBuckBoostInputsJson);
        auto inputs = isolatedBuckBoostInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        // Add output voltages/currents to each operating point
        // These come from the original IsolatedBuckBoost operating points
        if (isolatedBuckBoostInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                // Extract the original operating point index from the result
                // The pattern is: for each input voltage, we iterate through all original operating points
                size_t originalOpIdx = opIdx % isolatedBuckBoostInputs.get_operating_points().size();
                auto origOp = isolatedBuckBoostInputs.get_operating_points()[originalOpIdx];
                
                // Create output waveforms similar to excitations
                // For DC outputs, we create constant waveforms at the output voltage/current levels
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    // Extract frequency and duty cycle from first excitation
                    double frequency = 100000; // default
                    double dutyCycle = 0.5; // default
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        // Try to extract duty cycle from primary excitation
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;  // Switch on-time
                    double tOff = (1.0 - dutyCycle) * period;  // Switch off-time
                    int numPoints = 100;
                    
                    json voltageWaveform = {
                        {"time", json::array()},
                        {"data", json::array()}
                    };
                    
                    json currentWaveform = {
                        {"time", json::array()},
                        {"data", json::array()}
                    };
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    
                    // For primary output (index 0): create flyback-style pulsed current
                    // Current flows only during switch-off time (when diode conducts)
                    // Peak current is higher than DC to deliver same average power
                    if (i == 0) {
                        // Primary output current: zero during tOn, triangular ramp-down during tOff
                        double peakCurrent = outputCurrent / (1.0 - dutyCycle) * 2.0;  // Triangular average
                        
                        for (int j = 0; j < numPoints; j++) {
                            double t = (j / double(numPoints)) * period;
                            voltageWaveform["time"].push_back(t);
                            voltageWaveform["data"].push_back(outputVoltage);  // DC voltage
                            currentWaveform["time"].push_back(t);
                            
                            if (t < tOn) {
                                // Switch on: no output current (diode blocking)
                                currentWaveform["data"].push_back(0.0);
                            } else {
                                // Switch off: triangular ramp-down from peak to zero
                                double tInOffPeriod = t - tOn;
                                double current = peakCurrent * (1.0 - tInOffPeriod / tOff);
                                currentWaveform["data"].push_back(current);
                            }
                        }
                    } else {
                        // Secondary outputs (index 1+): also flyback-style pulsed current
                        double peakCurrent = outputCurrent / (1.0 - dutyCycle) * 2.0;
                        
                        for (int j = 0; j < numPoints; j++) {
                            double t = (j / double(numPoints)) * period;
                            voltageWaveform["time"].push_back(t);
                            voltageWaveform["data"].push_back(outputVoltage);  // DC voltage
                            currentWaveform["time"].push_back(t);
                            
                            if (t < tOn) {
                                currentWaveform["data"].push_back(0.0);
                            } else {
                                double tInOffPeriod = t - tOn;
                                double current = peakCurrent * (1.0 - tInOffPeriod / tOff);
                                currentWaveform["data"].push_back(current);
                            }
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_isolated_buck_boost_inputs(std::string isolatedBuckBoostInputsString){
    try {
        json isolatedBuckBoostInputsJson = json::parse(isolatedBuckBoostInputsString);

        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (isolatedBuckBoostInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = isolatedBuckBoostInputsJson["numberOfPeriods"].get<size_t>();
        }

        OpenMagnetics::AdvancedIsolatedBuckBoost isolatedBuckBoostInputs(isolatedBuckBoostInputsJson);
        auto inputs = isolatedBuckBoostInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        // Add output voltages/currents to each operating point
        if (isolatedBuckBoostInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % isolatedBuckBoostInputs.get_operating_points().size();
                auto origOp = isolatedBuckBoostInputs.get_operating_points()[originalOpIdx];
                
                // Create output waveforms similar to excitations
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    // Extract frequency and duty cycle from first excitation
                    double frequency = 100000; // default
                    double dutyCycle = 0.5; // default
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    double tOff = (1.0 - dutyCycle) * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {
                        {"time", json::array()},
                        {"data", json::array()}
                    };
                    
                    json currentWaveform = {
                        {"time", json::array()},
                        {"data", json::array()}
                    };
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    
                    // Create flyback-style pulsed current for all outputs
                    double peakCurrent = outputCurrent / (1.0 - dutyCycle) * 2.0;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            currentWaveform["data"].push_back(0.0);
                        } else {
                            double tInOffPeriod = t - tOn;
                            double current = peakCurrent * (1.0 - tInOffPeriod / tOff);
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_buck_inputs(std::string buckInputsString){
    try {
        json buckInputsJson = json::parse(buckInputsString);

        OpenMagnetics::Buck buckInputs(buckInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (buckInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = buckInputsJson["numberOfPeriods"].get<size_t>();
        }
        buckInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = buckInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_buck_inputs(std::string buckInputsString){
    try {
        json buckInputsJson = json::parse(buckInputsString);

        OpenMagnetics::AdvancedBuck buckInputs(buckInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (buckInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = buckInputsJson["numberOfPeriods"].get<size_t>();
        }
        buckInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = buckInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_boost_inputs(std::string boostInputsString){
    try {
        json boostInputsJson = json::parse(boostInputsString);

        OpenMagnetics::Boost boostInputs(boostInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (boostInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = boostInputsJson["numberOfPeriods"].get<size_t>();
        }
        boostInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = boostInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_boost_inputs(std::string boostInputsString){
    try {
        json boostInputsJson = json::parse(boostInputsString);

        OpenMagnetics::AdvancedBoost boostInputs(boostInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (boostInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = boostInputsJson["numberOfPeriods"].get<size_t>();
        }
        boostInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = boostInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_buck_ideal_waveforms(std::string buckInputsString){
    try {
        json buckInputsJson = json::parse(buckInputsString);

        // Detect if this is an AdvancedBuck (user knows design) or regular Buck (help with design)
        bool isAdvancedBuck = buckInputsJson.contains("desiredInductance");
        
        DesignRequirements designRequirements;
        double inductance;
        
        std::unique_ptr<OpenMagnetics::Buck> buckPtr;
        
        if (isAdvancedBuck) {
            auto advancedBuckPtr = std::make_unique<OpenMagnetics::AdvancedBuck>(buckInputsJson);
            inductance = advancedBuckPtr->get_desired_inductance();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(inductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            std::vector<IsolationSide> isolationSides;
            isolationSides.push_back(OpenMagnetics::get_isolation_side_from_index(0));
            designRequirements.set_isolation_sides(isolationSides);
            designRequirements.set_topology(Topologies::BUCK_CONVERTER);
            
            buckPtr = std::move(advancedBuckPtr);
        } else {
            buckPtr = std::make_unique<OpenMagnetics::Buck>(buckInputsJson);
            designRequirements = buckPtr->process_design_requirements();
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                inductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                inductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (buckInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = buckInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (buckInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = buckInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        // Set the number of periods to extract (not hardcoded to 1)
        buckPtr->set_num_periods_to_extract(numberOfPeriods);
        buckPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
        auto topologyWaveforms = buckPtr->simulate_and_extract_topology_waveforms(inductance);
        auto operatingPoints = buckPtr->simulate_and_extract_operating_points(inductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_boost_ideal_waveforms(std::string boostInputsString){
    try {
        json boostInputsJson = json::parse(boostInputsString);

        // Detect if this is an AdvancedBoost (user knows design) or regular Boost (help with design)
        bool isAdvancedBoost = boostInputsJson.contains("desiredInductance");
        
        DesignRequirements designRequirements;
        double inductance;
        
        std::unique_ptr<OpenMagnetics::Boost> boostPtr;
        
        if (isAdvancedBoost) {
            auto advancedBoostPtr = std::make_unique<OpenMagnetics::AdvancedBoost>(boostInputsJson);
            inductance = advancedBoostPtr->get_desired_inductance();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(inductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            std::vector<IsolationSide> isolationSides;
            isolationSides.push_back(OpenMagnetics::get_isolation_side_from_index(0));
            designRequirements.set_isolation_sides(isolationSides);
            designRequirements.set_topology(Topologies::BOOST_CONVERTER);
            
            boostPtr = std::move(advancedBoostPtr);
        } else {
            boostPtr = std::make_unique<OpenMagnetics::Boost>(boostInputsJson);
            designRequirements = boostPtr->process_design_requirements();
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                inductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                inductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (boostInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = boostInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (boostInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = boostInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        // Set the number of periods to extract (not hardcoded to 1)
        boostPtr->set_num_periods_to_extract(numberOfPeriods);
        boostPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
        auto topologyWaveforms = boostPtr->simulate_and_extract_topology_waveforms(inductance);
        auto operatingPoints = boostPtr->simulate_and_extract_operating_points(inductance);
        
        // DEBUG: Log operating point structure
        std::cerr << "DEBUG [simulate_boost]: Operating points count = " << operatingPoints.size() << std::endl;
        for (size_t opIdx = 0; opIdx < operatingPoints.size(); ++opIdx) {
            const auto& op = operatingPoints[opIdx];
            std::cerr << "  OP " << opIdx << ": excitations count = " << op.get_excitations_per_winding().size() << std::endl;
            for (size_t excIdx = 0; excIdx < op.get_excitations_per_winding().size(); ++excIdx) {
                const auto& exc = op.get_excitations_per_winding()[excIdx];
                std::cerr << "    Excitation " << excIdx << ":";
                if (exc.get_voltage() && exc.get_voltage()->get_waveform()) {
                    const auto vWf = exc.get_voltage()->get_waveform().value();  // Fix dangling reference
                    std::cerr << " voltage=" << vWf.get_data().size() << "pts";
                    if (!vWf.get_data().empty()) {
                        double minV = *std::min_element(vWf.get_data().begin(), vWf.get_data().end());
                        double maxV = *std::max_element(vWf.get_data().begin(), vWf.get_data().end());
                        std::cerr << " [" << minV << ".." << maxV << "V]";
                    }
                } else {
                    std::cerr << " voltage=none";
                }
                if (exc.get_current() && exc.get_current()->get_waveform()) {
                    const auto iWf = exc.get_current()->get_waveform().value();  // Fix dangling reference
                    std::cerr << " current=" << iWf.get_data().size() << "pts";
                    if (!iWf.get_data().empty()) {
                        double minI = *std::min_element(iWf.get_data().begin(), iWf.get_data().end());
                        double maxI = *std::max_element(iWf.get_data().begin(), iWf.get_data().end());
                        std::cerr << " [" << minI << ".." << maxI << "A]";
                    }
                } else {
                    std::cerr << " current=none";
                }
                std::cerr << std::endl;
            }
        }
         
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_forward_ideal_waveforms(std::string forwardInputsString){
    try {
        json forwardInputsJson = json::parse(forwardInputsString);

        // Detect if this is an AdvancedSingleSwitchForward or regular
        bool isAdvanced = forwardInputsJson.contains("desiredInductance");
        
        // Read number of periods from input (default to 1)
        size_t numberOfPeriods = 1;
        if (forwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = forwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (forwardInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = forwardInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::SingleSwitchForward> forwardPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedSingleSwitchForward>(forwardInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::SINGLE_SWITCH_FORWARD_CONVERTER);
            
            forwardPtr = std::move(advancedPtr);
        } else {
            forwardPtr = std::make_unique<OpenMagnetics::SingleSwitchForward>(forwardInputsJson);
            designRequirements = forwardPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
        forwardPtr->set_num_periods_to_extract(numberOfPeriods);
        forwardPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        auto topologyWaveforms = forwardPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance);
        auto operatingPoints = forwardPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_two_switch_forward_ideal_waveforms(std::string forwardInputsString){
    try {
        json forwardInputsJson = json::parse(forwardInputsString);

        // Detect if this is an AdvancedTwoSwitchForward or regular
        bool isAdvanced = forwardInputsJson.contains("desiredInductance");
        
        // Read number of periods from input (default to 1)
        size_t numberOfPeriods = 1;
        if (forwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = forwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (forwardInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = forwardInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        std::cerr << "DEBUG: TwoSwitchForward numberOfSteadyStatePeriods=" << numberOfSteadyStatePeriods << std::endl;
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::TwoSwitchForward> forwardPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedTwoSwitchForward>(forwardInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::TWO_SWITCH_FORWARD_CONVERTER);
            
            forwardPtr = std::move(advancedPtr);
        } else {
            forwardPtr = std::make_unique<OpenMagnetics::TwoSwitchForward>(forwardInputsJson);
            designRequirements = forwardPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            magnetizingInductance = OpenMagnetics::resolve_dimensional_values(designRequirements.get_magnetizing_inductance());
        }
        
        forwardPtr->set_num_periods_to_extract(numberOfPeriods);
        forwardPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        auto topologyWaveforms = forwardPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance);
        auto operatingPoints = forwardPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_active_clamp_forward_ideal_waveforms(std::string forwardInputsString){
    try {
        json forwardInputsJson = json::parse(forwardInputsString);

        // Detect if this is an AdvancedActiveClampForward or regular
        bool isAdvanced = forwardInputsJson.contains("desiredInductance");
        
        // Read number of periods from input (default to 1)
        size_t numberOfPeriods = 1;
        if (forwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = forwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (forwardInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = forwardInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::ActiveClampForward> forwardPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedActiveClampForward>(forwardInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::ACTIVE_CLAMP_FORWARD_CONVERTER);
            
            forwardPtr = std::move(advancedPtr);
        } else {
            forwardPtr = std::make_unique<OpenMagnetics::ActiveClampForward>(forwardInputsJson);
            designRequirements = forwardPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            magnetizingInductance = OpenMagnetics::resolve_dimensional_values(designRequirements.get_magnetizing_inductance());
        }
        
        forwardPtr->set_num_periods_to_extract(numberOfPeriods);
        forwardPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        auto topologyWaveforms = forwardPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance);
        auto operatingPoints = forwardPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_push_pull_ideal_waveforms(std::string pushPullInputsString){
    try {
        json pushPullInputsJson = json::parse(pushPullInputsString);

        // Detect if this is an AdvancedPushPull or regular
        bool isAdvanced = pushPullInputsJson.contains("desiredInductance");
        
        // Debug: Log what was received for desiredTurnsRatios
        if (isAdvanced && pushPullInputsJson.contains("desiredTurnsRatios")) {
            std::cerr << "DEBUG [simulate_push_pull]: desiredTurnsRatios in JSON = " << pushPullInputsJson["desiredTurnsRatios"].dump() << std::endl;
        }
        
        // Read number of periods from input (default to 1)
        size_t numberOfPeriods = 1;
        if (pushPullInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = pushPullInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (pushPullInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = pushPullInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::PushPull> pushPullPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedPushPull>(pushPullInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Debug: Log extracted values
            std::cerr << "DEBUG [simulate_push_pull_ideal_waveforms]: Advanced mode" << std::endl;
            std::cerr << "  magnetizingInductance = " << magnetizingInductance << std::endl;
            std::cerr << "  turnsRatios.size() = " << turnsRatios.size() << std::endl;
            for (size_t i = 0; i < turnsRatios.size(); i++) {
                std::cerr << "  turnsRatios[" << i << "] = " << turnsRatios[i] << std::endl;
            }
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::PUSH_PULL_CONVERTER);
            
            pushPullPtr = std::move(advancedPtr);
        } else {
            pushPullPtr = std::make_unique<OpenMagnetics::PushPull>(pushPullInputsJson);
            designRequirements = pushPullPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
        // Set steady state periods
        pushPullPtr->set_num_periods_to_extract(numberOfPeriods);
        pushPullPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        // CRITICAL FIX: Convert turns ratios from user format [N_sec/N_pri] to internal format [1, N_sec/N_pri, N_sec/N_pri, ...]
        // The ngspice netlist generation expects the internal format where:
        //   - index 0: second primary (always 1)
        //   - index 1,2: first and second secondary (same value for main output)
        //   - index 3+: auxiliary secondaries (if any)
        std::vector<double> convertedTurnsRatios;
        convertedTurnsRatios.push_back(1.0);  // Second primary
        convertedTurnsRatios.push_back(turnsRatios[0]);  // First secondary
        convertedTurnsRatios.push_back(turnsRatios[0]);  // Second secondary
        for (size_t i = 1; i < turnsRatios.size(); ++i) {
            convertedTurnsRatios.push_back(turnsRatios[i]);  // Auxiliary secondaries
        }
        
        std::cerr << "DEBUG: After convert_turns_ratios, size = " << convertedTurnsRatios.size() << std::endl;
        for (size_t i = 0; i < convertedTurnsRatios.size(); i++) {
            std::cerr << "  convertedTurnsRatios[" << i << "] = " << convertedTurnsRatios[i] << std::endl;
        }
        
        auto topologyWaveforms = pushPullPtr->simulate_and_extract_topology_waveforms(convertedTurnsRatios, magnetizingInductance);
        auto operatingPoints = pushPullPtr->simulate_and_extract_operating_points(convertedTurnsRatios, magnetizingInductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_isolated_buck_boost_ideal_waveforms(std::string ibbInputsString){
    try {
        json ibbInputsJson = json::parse(ibbInputsString);

        // Detect if this is an AdvancedIsolatedBuckBoost or regular
        bool isAdvanced = ibbInputsJson.contains("desiredInductance");
        
        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (ibbInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = ibbInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (ibbInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = ibbInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::IsolatedBuckBoost> ibbPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedIsolatedBuckBoost>(ibbInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::ISOLATED_BUCK_BOOST_CONVERTER);
            
            ibbPtr = std::move(advancedPtr);
        } else {
            ibbPtr = std::make_unique<OpenMagnetics::IsolatedBuckBoost>(ibbInputsJson);
            designRequirements = ibbPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
        // Set steady state periods and extraction periods after pointer creation
        ibbPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        ibbPtr->set_num_periods_to_extract(numberOfPeriods);
        
#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif
        
        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }
        
        auto topologyWaveforms = ibbPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance);
        auto operatingPoints = ibbPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance);
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_isolated_buck_ideal_waveforms(std::string ibInputsString){
    try {
        json ibInputsJson = json::parse(ibInputsString);

        // Detect if this is an AdvancedIsolatedBuck or regular
        bool isAdvanced = ibInputsJson.contains("desiredInductance");
        
        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (ibInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = ibInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 5)
        size_t numberOfSteadyStatePeriods = 5;
        if (ibInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = ibInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        DesignRequirements designRequirements;
        double magnetizingInductance;
        std::vector<double> turnsRatios;
        
        std::unique_ptr<OpenMagnetics::IsolatedBuck> ibPtr;
        
        if (isAdvanced) {
            auto advancedPtr = std::make_unique<OpenMagnetics::AdvancedIsolatedBuck>(ibInputsJson);
            magnetizingInductance = advancedPtr->get_desired_inductance();
            turnsRatios = advancedPtr->get_desired_turns_ratios();
            
            // Build designRequirements
            designRequirements.get_mutable_turns_ratios().clear();
            for (auto tr : turnsRatios) {
                DimensionWithTolerance trWithTolerance;
                trWithTolerance.set_nominal(tr);
                designRequirements.get_mutable_turns_ratios().push_back(trWithTolerance);
            }
            DimensionWithTolerance inductanceWithTolerance;
            inductanceWithTolerance.set_nominal(magnetizingInductance);
            designRequirements.set_magnetizing_inductance(inductanceWithTolerance);
            designRequirements.set_topology(Topologies::ISOLATED_BUCK_CONVERTER);
            
            ibPtr = std::move(advancedPtr);
        } else {
            ibPtr = std::make_unique<OpenMagnetics::IsolatedBuck>(ibInputsJson);
            designRequirements = ibPtr->process_design_requirements();
            
            // Extract turns ratios from design requirements
            for (const auto& tr : designRequirements.get_turns_ratios()) {
                if (tr.get_nominal()) {
                    turnsRatios.push_back(tr.get_nominal().value());
                }
            }
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                magnetizingInductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate inductance");
            }
        }
        
        // Set the number of periods for extraction
        ibPtr->set_num_periods_to_extract(numberOfPeriods);
        ibPtr->set_num_steady_state_periods(numberOfSteadyStatePeriods);
        
        // Get operating points from ngspice simulation (already has correct number of periods)
        auto operatingPoints = ibPtr->simulate_and_extract_operating_points(turnsRatios, magnetizingInductance);
        
        // DEBUG: Log operating point structure
        std::cerr << "DEBUG [simulate_isolated_buck]: Operating points count = " << operatingPoints.size() << std::endl;
        for (size_t opIdx = 0; opIdx < operatingPoints.size(); ++opIdx) {
            const auto& op = operatingPoints[opIdx];
            std::cerr << "  OP " << opIdx << ": excitations count = " << op.get_excitations_per_winding().size() << std::endl;
            for (size_t excIdx = 0; excIdx < op.get_excitations_per_winding().size(); ++excIdx) {
                const auto& exc = op.get_excitations_per_winding()[excIdx];
                std::cerr << "    Excitation " << excIdx << ":";
                if (exc.get_voltage() && exc.get_voltage()->get_waveform()) {
                    std::cerr << " voltage=" << exc.get_voltage()->get_waveform()->get_data().size() << "pts";
                } else {
                    std::cerr << " voltage=none";
                }
                if (exc.get_current() && exc.get_current()->get_waveform()) {
                    std::cerr << " current=" << exc.get_current()->get_waveform()->get_data().size() << "pts";
                } else {
                    std::cerr << " current=none";
                }
                std::cerr << std::endl;
            }
        }
        
        // Note: ngspice already extracted the correct number of periods, no need to repeat
        
        // Build the result with just two fields: inputs and converterWaveforms
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        auto topologyWaveforms = ibPtr->simulate_and_extract_topology_waveforms(turnsRatios, magnetizingInductance);
        
        // DEBUG: Log converter waveforms structure
        std::cerr << "DEBUG [simulate_isolated_buck]: Topology waveforms count = " << topologyWaveforms.size() << std::endl;
        for (size_t twIdx = 0; twIdx < topologyWaveforms.size(); ++twIdx) {
            const auto& tw = topologyWaveforms[twIdx];
            std::cerr << "  TW " << twIdx << ":";
            if (tw.get_input_voltage().get_data().size() > 0) {
                std::cerr << " input_voltage=" << tw.get_input_voltage().get_data().size() << "pts";
            } else {
                std::cerr << " input_voltage=none";
            }
            std::cerr << " output_voltages=" << tw.get_output_voltages().size();
            std::cerr << " output_currents=" << tw.get_output_currents().size();
            std::cerr << std::endl;
            for (size_t outIdx = 0; outIdx < tw.get_output_voltages().size(); ++outIdx) {
                std::cerr << "    Output " << outIdx << ": voltage=" << tw.get_output_voltages()[outIdx].get_data().size() << "pts";
                if (outIdx < tw.get_output_currents().size()) {
                    std::cerr << " current=" << tw.get_output_currents()[outIdx].get_data().size() << "pts";
                }
                std::cerr << std::endl;
            }
        }
        
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_push_pull_inputs(std::string pushPullInputsString){
    try {
        json pushPullInputsJson = json::parse(pushPullInputsString);

        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (pushPullInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = pushPullInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Debug: Log current ripple ratio
        std::cerr << "DEBUG [calculate_push_pull_inputs]: currentRippleRatio = ";
        if (pushPullInputsJson.contains("currentRippleRatio")) {
            std::cerr << pushPullInputsJson["currentRippleRatio"].get<double>();
        } else {
            std::cerr << "not set";
        }
        std::cerr << std::endl;

        OpenMagnetics::PushPull pushPullInputs(pushPullInputsJson);
        auto inputs = pushPullInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << "ERROR [calculate_push_pull_inputs]: " << exc.what() << std::endl;
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

std::string calculate_advanced_push_pull_inputs(std::string pushPullInputsString){
    try {
        json pushPullInputsJson = json::parse(pushPullInputsString);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (pushPullInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = pushPullInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Debug: Log received parameters
        std::cerr << "DEBUG [calculate_advanced_push_pull_inputs]: Received JSON with keys: ";
        for (auto& el : pushPullInputsJson.items()) {
            std::cerr << el.key() << ", ";
        }
        std::cerr << std::endl;
        
        // Debug: Log the critical values
        if (pushPullInputsJson.contains("desiredTurnsRatios")) {
            std::cerr << "  desiredTurnsRatios = " << pushPullInputsJson["desiredTurnsRatios"].dump() << std::endl;
        }
        if (pushPullInputsJson.contains("desiredDutyCycle")) {
            std::cerr << "  desiredDutyCycle = " << pushPullInputsJson["desiredDutyCycle"].dump() << std::endl;
        }
        if (pushPullInputsJson.contains("desiredInductance")) {
            std::cerr << "  desiredInductance = " << pushPullInputsJson["desiredInductance"] << std::endl;
        }
        if (pushPullInputsJson.contains("inputVoltage")) {
            std::cerr << "  inputVoltage = " << pushPullInputsJson["inputVoltage"].dump() << std::endl;
        }
        
        // IMPORTANT: The analytical process() method expects user-format turns ratios [N]
        // and internally converts them to [1, N, N, ...] for calculations.
        // So we do NOT convert here - just pass through as-is.
        // The JSON already has the correct format from the frontend.

        OpenMagnetics::AdvancedPushPull pushPullInputs(pushPullInputsJson);
        auto inputs = pushPullInputs.process();

        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cerr << "ERROR [calculate_advanced_push_pull_inputs]: " << exc.what() << std::endl;
        json error;
        error["error"] = true;
        error["message"] = exc.what();
        return error.dump(4);
    }
}

std::string calculate_single_switch_forward_inputs(std::string singleSwitchForwardInputsString){
    try {
        json singleSwitchForwardInputsJson = json::parse(singleSwitchForwardInputsString);

        OpenMagnetics::SingleSwitchForward singleSwitchForwardInputs(singleSwitchForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (singleSwitchForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = singleSwitchForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        singleSwitchForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = singleSwitchForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (singleSwitchForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % singleSwitchForwardInputs.get_operating_points().size();
                auto origOp = singleSwitchForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_single_switch_forward_inputs(std::string singleSwitchForwardInputsString){
    try {
        json singleSwitchForwardInputsJson = json::parse(singleSwitchForwardInputsString);

        OpenMagnetics::AdvancedSingleSwitchForward singleSwitchForwardInputs(singleSwitchForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (singleSwitchForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = singleSwitchForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        singleSwitchForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = singleSwitchForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (singleSwitchForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % singleSwitchForwardInputs.get_operating_points().size();
                auto origOp = singleSwitchForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error = {
            {"error", true},
            {"message", std::string{exc.what()}}
        };
        return error.dump(4);
    }
}

std::string calculate_active_clamp_forward_inputs(std::string activeClampForwardInputsString){
    try {
        json activeClampForwardInputsJson = json::parse(activeClampForwardInputsString);

        OpenMagnetics::ActiveClampForward activeClampForwardInputs(activeClampForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (activeClampForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = activeClampForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        activeClampForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = activeClampForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (activeClampForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % activeClampForwardInputs.get_operating_points().size();
                auto origOp = activeClampForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_active_clamp_forward_inputs(std::string activeClampForwardInputsString){
    try {
        json activeClampForwardInputsJson = json::parse(activeClampForwardInputsString);

        OpenMagnetics::AdvancedActiveClampForward activeClampForwardInputs(activeClampForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (activeClampForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = activeClampForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        activeClampForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = activeClampForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (activeClampForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % activeClampForwardInputs.get_operating_points().size();
                auto origOp = activeClampForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error = {
            {"error", true},
            {"message", std::string{exc.what()}}
        };
        return error.dump(4);
    }
}

std::string calculate_two_switch_forward_inputs(std::string twoSwitchForwardInputsString){
    try {
        json twoSwitchForwardInputsJson = json::parse(twoSwitchForwardInputsString);

        OpenMagnetics::TwoSwitchForward twoSwitchForwardInputs(twoSwitchForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (twoSwitchForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = twoSwitchForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        twoSwitchForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = twoSwitchForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (twoSwitchForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % twoSwitchForwardInputs.get_operating_points().size();
                auto origOp = twoSwitchForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_two_switch_forward_inputs(std::string twoSwitchForwardInputsString){
    try {
        json twoSwitchForwardInputsJson = json::parse(twoSwitchForwardInputsString);

        OpenMagnetics::AdvancedTwoSwitchForward twoSwitchForwardInputs(twoSwitchForwardInputsJson);

        // Read number of periods from input
        size_t numberOfPeriods = 2;
        if (twoSwitchForwardInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = twoSwitchForwardInputsJson["numberOfPeriods"].get<size_t>();
        }
        twoSwitchForwardInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = twoSwitchForwardInputs.process();

        json result;
        to_json(result, inputs);
        
        // Add output voltages/currents to each operating point
        if (twoSwitchForwardInputs.get_operating_points().size() > 0 && result.contains("operatingPoints")) {
            size_t opIdx = 0;
            for (auto& op : result["operatingPoints"]) {
                size_t originalOpIdx = opIdx % twoSwitchForwardInputs.get_operating_points().size();
                auto origOp = twoSwitchForwardInputs.get_operating_points()[originalOpIdx];
                
                json outputVoltagesArray = json::array();
                json outputCurrentsArray = json::array();
                
                for (size_t i = 0; i < origOp.get_output_voltages().size(); i++) {
                    double frequency = 100000;
                    double dutyCycle = 0.5;
                    if (op.contains("excitationsPerWinding") && op["excitationsPerWinding"].size() > 0) {
                        if (op["excitationsPerWinding"][0].contains("frequency")) {
                            frequency = op["excitationsPerWinding"][0]["frequency"];
                        }
                        if (op["excitationsPerWinding"][0].contains("current") && 
                            op["excitationsPerWinding"][0]["current"].contains("processed") &&
                            op["excitationsPerWinding"][0]["current"]["processed"].contains("dutyCycle")) {
                            dutyCycle = op["excitationsPerWinding"][0]["current"]["processed"]["dutyCycle"];
                        }
                    }
                    
                    double period = 1.0 / frequency;
                    double tOn = dutyCycle * period;
                    int numPoints = 100;
                    
                    json voltageWaveform = {{"time", json::array()}, {"data", json::array()}};
                    json currentWaveform = {{"time", json::array()}, {"data", json::array()}};
                    
                    double outputVoltage = origOp.get_output_voltages()[i];
                    double outputCurrent = origOp.get_output_currents()[i];
                    double currentRipple = outputCurrent * 0.3;
                    double minCurrent = outputCurrent - currentRipple / 2;
                    double maxCurrent = outputCurrent + currentRipple / 2;
                    
                    for (int j = 0; j < numPoints; j++) {
                        double t = (j / double(numPoints)) * period;
                        voltageWaveform["time"].push_back(t);
                        voltageWaveform["data"].push_back(outputVoltage);
                        currentWaveform["time"].push_back(t);
                        
                        if (t < tOn) {
                            double progress = t / tOn;
                            double current = minCurrent + (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        } else {
                            double progress = (t - tOn) / (period - tOn);
                            double current = maxCurrent - (maxCurrent - minCurrent) * progress;
                            currentWaveform["data"].push_back(current);
                        }
                    }
                    
                    outputVoltagesArray.push_back({{"waveform", voltageWaveform}});
                    outputCurrentsArray.push_back({{"waveform", currentWaveform}});
                }
                
                op["outputVoltages"] = outputVoltagesArray;
                op["outputCurrents"] = outputCurrentsArray;
                opIdx++;
            }
        }

        // Repeat waveforms for the requested number of periods
        if (result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error = {
            {"error", true},
            {"message", std::string{exc.what()}}
        };
        return error.dump(4);
    }
}

// ==========================================
// Power Factor Correction (PFC) Functions
// ==========================================

EMSCRIPTEN_KEEPALIVE std::string calculate_pfc_inputs(std::string pfcInputsString){
    try {
        json pfcInputsJson = json::parse(pfcInputsString);

        OpenMagnetics::PowerFactorCorrection pfcInputs(pfcInputsJson);
        
        // Set number of periods for waveform generation
        int numberOfPeriods = pfcInputsJson.value("numberOfPeriods", 2);
        // pfcInputs.set_number_of_periods(numberOfPeriods);
        
        auto designRequirements = pfcInputs.process_design_requirements();
        
        // Calculate inductance based on mode
        double inductance;
        if (pfcInputsJson.contains("inductance")) {
            inductance = pfcInputsJson["inductance"];
        } else {
            std::string mode = pfcInputsJson.value("mode", "Continuous Conduction Mode");
            if (mode == "Continuous Conduction Mode") {
                inductance = pfcInputs.calculate_inductance_ccm();
            } else if (mode == "Critical Conduction Mode") {
                inductance = pfcInputs.calculate_inductance_crcm();
            } else {
                inductance = pfcInputs.calculate_inductance_dcm();
            }
        }
        
        // Get operating points at different AC line phases (single period for MAS)
        auto operatingPoints = pfcInputs.process_operating_points({}, inductance);
        
        // Get full multi-period waveforms for display (respects numberOfPeriods setting)
        // Note: simulate_and_extract_waveforms already generates the requested number of periods
        auto displayWaveforms = pfcInputs.simulate_and_extract_waveforms(inductance, 0.1, numberOfPeriods);
        
        // Build result with inputs and waveforms
        json result;
        to_json(result, designRequirements);
        result["inductance"] = inductance;
        result["operatingPoints"] = json::array();
        
        for (auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            result["operatingPoints"].push_back(opJson);
        }
        
        // Note: No need to call repeat_operating_points_waveforms here because
        // simulate_and_extract_waveforms already generated the correct number of periods
        
        // Build MAS inputs - only include first operating point for magnetic design
        result["masInputs"] = json::object();
        to_json(result["masInputs"], designRequirements);
        // Only export first operating point to magnetic tool to avoid redundant calculations
        if (!result["operatingPoints"].empty()) {
            result["masInputs"]["operatingPoints"] = json::array({result["operatingPoints"][0]});
        } else {
            result["masInputs"]["operatingPoints"] = json::array();
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

EMSCRIPTEN_KEEPALIVE std::string simulate_pfc_waveforms(std::string pfcInputsString){
    try {
        json pfcInputsJson = json::parse(pfcInputsString);

        OpenMagnetics::PowerFactorCorrection pfcInputs(pfcInputsJson);
        
        // Calculate inductance if not provided
        double inductance;
        if (pfcInputsJson.contains("inductance")) {
            inductance = pfcInputsJson["inductance"];
        } else {
            std::string mode = pfcInputsJson.value("mode", "Continuous Conduction Mode");
            if (mode == "Continuous Conduction Mode") {
                inductance = pfcInputs.calculate_inductance_ccm();
            } else if (mode == "Critical Conduction Mode") {
                inductance = pfcInputs.calculate_inductance_crcm();
            } else {
                inductance = pfcInputs.calculate_inductance_dcm();
            }
        }
        
        // Get design requirements
        auto designRequirements = pfcInputs.process_design_requirements();
        
        // Get simulation parameters
        double dcResistance = pfcInputsJson.value("dcResistance", 0.1);
        int numberOfCycles = pfcInputsJson.value("numberOfPeriods", 2);
        
        // Set the number of periods for operating point extraction
        pfcInputs.set_num_periods_to_extract(numberOfCycles);
        
        // Simulate and extract waveforms
        auto simWaveforms = pfcInputs.simulate_and_extract_waveforms(
            inductance,
            dcResistance,
            numberOfCycles
        );
        
        // Get operating points from simulation (this will have multiple periods)
        auto operatingPoints = pfcInputs.simulate_and_extract_operating_points(inductance, dcResistance);
        
        // Build result
        json result;
        result["inductance"] = inductance;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        result["converterWaveforms"] = json::array();
        
        // Converter waveforms (for circuit analysis) - Standard format matching other topologies
        json convOp;
        convOp["operatingPointName"] = simWaveforms.operatingPointName;
        convOp["switchingFrequency"] = simWaveforms.switchingFrequency;
        
        // Input voltage (rectified AC) - Standard format with .time and .data
        if (!simWaveforms.inputVoltage.empty()) {
            json inputVoltage;
            inputVoltage["time"] = simWaveforms.time;
            inputVoltage["data"] = simWaveforms.inputVoltage;
            convOp["inputVoltage"] = inputVoltage;
        }
        
        // Input current (inductor current) - Standard format with .time and .data
        if (!simWaveforms.inputCurrent.empty()) {
            json inputCurrent;
            inputCurrent["time"] = simWaveforms.time;
            inputCurrent["data"] = simWaveforms.inputCurrent;
            convOp["inputCurrent"] = inputCurrent;
        }
        
        // Output current (averaged inductor current for PFC) - Standard format
        if (!simWaveforms.outputCurrent.empty()) {
            json outputCurrent;
            outputCurrent["time"] = simWaveforms.time;
            outputCurrent["data"] = simWaveforms.outputCurrent;
            convOp["outputCurrents"] = json::array({outputCurrent});
        }
        
        result["converterWaveforms"].push_back(convOp);
        
        // Add PFC metrics
        result["powerFactor"] = simWaveforms.powerFactor;
        result["efficiency"] = simWaveforms.efficiency;
        result["currentThd"] = simWaveforms.currentThd;
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

EMSCRIPTEN_KEEPALIVE std::string determine_pfc_mode(std::string pfcInputsString, double inductance){
    try {
        json pfcInputsJson = json::parse(pfcInputsString);
        OpenMagnetics::PowerFactorCorrection pfcInputs(pfcInputsJson);
        
        std::string actualMode = pfcInputs.determine_actual_mode(inductance);
        
        json result;
        result["actualMode"] = actualMode;
        
        // Also return critical inductance for reference
        result["criticalInductance"] = pfcInputs.calculate_inductance_crcm();
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

// ==========================================
// Common Mode Choke (CMC) Functions
// ==========================================

std::string calculate_cmc_inputs(std::string cmcInputsString){
    try {
        json cmcInputsJson = json::parse(cmcInputsString);

        OpenMagnetics::CommonModeChoke cmcInputs(cmcInputsJson);

        size_t numberOfPeriods = 1;
        if (cmcInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = cmcInputsJson["numberOfPeriods"].get<size_t>();
        }
        cmcInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = cmcInputs.process();

        json result;
        to_json(result, inputs);

        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advanced_cmc_inputs(std::string cmcInputsString){
    try {
        json cmcInputsJson = json::parse(cmcInputsString);

        OpenMagnetics::AdvancedCommonModeChoke cmcInputs(cmcInputsJson);

        size_t numberOfPeriods = 1;
        if (cmcInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = cmcInputsJson["numberOfPeriods"].get<size_t>();
        }
        cmcInputs.set_num_periods_to_extract(numberOfPeriods);

        auto inputs = cmcInputs.process();

        json result;
        to_json(result, inputs);

        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }

        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// CMC SPICE Circuit Generation
EMSCRIPTEN_KEEPALIVE std::string generate_cmc_ngspice_circuit(std::string cmcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex){
    try {
        json cmcInputsJson = json::parse(cmcInputsString);
        
        bool isAdvancedCmc = cmcInputsJson.contains("desiredInductance");
        
        std::unique_ptr<OpenMagnetics::CommonModeChoke> cmcPtr;
        double inductance;
        double frequency = 150000; // Default frequency for CMC
        
        if (isAdvancedCmc) {
            auto advancedCmcPtr = std::make_unique<OpenMagnetics::AdvancedCommonModeChoke>(cmcInputsJson);
            inductance = advancedCmcPtr->get_desired_inductance();
            cmcPtr = std::move(advancedCmcPtr);
        } else {
            cmcPtr = std::make_unique<OpenMagnetics::CommonModeChoke>(cmcInputsJson);
            auto designRequirements = cmcPtr->process_design_requirements();
            
            if (designRequirements.get_magnetizing_inductance().get_minimum()) {
                inductance = designRequirements.get_magnetizing_inductance().get_minimum().value();
            } else if (designRequirements.get_magnetizing_inductance().get_nominal()) {
                inductance = designRequirements.get_magnetizing_inductance().get_nominal().value();
            } else {
                throw std::runtime_error("Unable to calculate CMC inductance");
            }
        }
        
        // CMC generate_ngspice_circuit takes (inductance, frequency) - ignore indices
        std::string netlist = cmcPtr->generate_ngspice_circuit(inductance, frequency);
        return netlist;
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// CMC LISN Test - runs ngspice with standardized CISPR test circuit
EMSCRIPTEN_KEEPALIVE std::string simulate_cmc_lisn_waveforms(std::string cmcInputsString, double inductance) {
    try {
        json cmcInputsJson = json::parse(cmcInputsString);
        
        OpenMagnetics::CommonModeChoke cmc(cmcInputsJson);
        
        // Get design requirements
        auto designRequirements = cmc.process_design_requirements();
        
        // Get test frequencies from impedance points
        std::vector<double> frequencies;
        if (cmcInputsJson.contains("impedancePoints") && cmcInputsJson["impedancePoints"].is_array()) {
            for (const auto& point : cmcInputsJson["impedancePoints"]) {
                if (point.contains("frequency")) {
                    frequencies.push_back(point["frequency"].get<double>());
                }
            }
        }
        
        // If no frequencies specified, use default
        if (frequencies.empty()) {
            frequencies.push_back(150000); // 150 kHz default
        }
        
        // Run simulation
        auto waveforms = cmc.simulate_and_extract_waveforms(inductance, frequencies);
        
        // Also get operating points for the magnetic data
        auto operatingPoints = cmc.simulate_and_extract_operating_points(inductance);
        
        // Build the result with inputs and converterWaveforms (same format as other wizards)
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: array of CMC test waveforms for visualization
        result["converterWaveforms"] = json::array();
        for (const auto& wf : waveforms) {
            json cwJson;
            cwJson["frequency"] = wf.frequency;
            cwJson["time"] = wf.time;
            cwJson["inputVoltage"] = wf.inputVoltage;
            cwJson["windingCurrents"] = wf.windingCurrents;
            cwJson["lisnVoltage"] = wf.lisnVoltage;
            cwJson["operatingPointName"] = wf.operatingPointName;
            cwJson["commonModeAttenuation"] = wf.commonModeAttenuation;
            cwJson["commonModeImpedance"] = wf.commonModeImpedance;
            cwJson["theoreticalImpedance"] = wf.theoreticalImpedance;
            result["converterWaveforms"].push_back(cwJson);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// CMC Ideal Waveforms - realistic line voltage + switching noise for design
EMSCRIPTEN_KEEPALIVE std::string simulate_cmc_ideal_waveforms(std::string cmcInputsString, double inductance, double parasiticCap_pF, double dvdt_V_ns) {
    try {
        json cmcInputsJson = json::parse(cmcInputsString);
        
        OpenMagnetics::CommonModeChoke cmc(cmcInputsJson);
        
        // Get design requirements
        auto designRequirements = cmc.process_design_requirements();
        
        // Run realistic simulation with line + noise
        auto operatingPoints = cmc.simulate_realistic_cmc(inductance, parasiticCap_pF, dvdt_V_ns);
        
        // Build the result with inputs and converterWaveforms (same format as other wizards)
        json result;
        
        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputs;
        inputs["designRequirements"] = json();
        to_json(inputs["designRequirements"], designRequirements);
        inputs["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputs["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputs;
        
        // converterWaveforms: empty array for CMC (realistic simulation doesn't generate converter-style waveforms)
        result["converterWaveforms"] = json::array();
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

// ==========================================
// Differential Mode Choke (DMC) Functions
// ==========================================

std::string calculate_dmc_inputs(std::string dmcInputsString){
    try {
        json dmcInputsJson = json::parse(dmcInputsString);

        OpenMagnetics::DifferentialModeChoke dmcInputs(dmcInputsJson);
        auto inputs = dmcInputs.process();

        json result;
        to_json(result, inputs);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string verify_dmc_attenuation(std::string dmcInputsString, double inductance, double capacitance) {
    try {
        json dmcInputsJson = json::parse(dmcInputsString);
        OpenMagnetics::DifferentialModeChoke dmc(dmcInputsJson);

        std::optional<double> cap = (capacitance > 0) ? std::optional<double>(capacitance) : std::nullopt;
        auto results = dmc.verify_attenuation(inductance, cap);

        json result = json::array();
        for (const auto& r : results) {
            json rJson;
            rJson["frequency"] = r.frequency;
            rJson["requiredAttenuation"] = r.requiredAttenuation;
            rJson["measuredAttenuation"] = r.measuredAttenuation;
            rJson["theoreticalAttenuation"] = r.theoreticalAttenuation;
            rJson["passed"] = r.passed;
            rJson["message"] = r.message;
            result.push_back(rJson);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string propose_dmc_design(std::string dmcInputsString) {
    try {
        json dmcInputsJson = json::parse(dmcInputsString);
        OpenMagnetics::DifferentialModeChoke dmc(dmcInputsJson);

        auto proposal = dmc.propose_design();
        return proposal.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_dmc_waveforms(std::string dmcInputsString, double inductance) {
    try {
        json dmcInputsJson = json::parse(dmcInputsString);
        OpenMagnetics::DifferentialModeChoke dmc(dmcInputsJson);

        // Get test frequencies from minimum impedance requirements
        std::vector<double> frequencies;
        auto minimumImpedance = dmc.get_minimum_impedance();
        if (minimumImpedance) {
            for (const auto& imp : *minimumImpedance) {
                frequencies.push_back(imp.get_frequency());
            }
        }
        
        // If no impedance points specified, use default EMI test frequencies
        if (frequencies.empty()) {
            frequencies = {150000, 500000, 1000000, 10000000, 30000000};
        }

        auto waveforms = dmc.simulate_and_extract_waveforms(inductance, frequencies);

        json result = json::array();
        for (const auto& wf : waveforms) {
            json wfJson;
            wfJson["time"] = wf.time;
            wfJson["frequency"] = wf.frequency;
            wfJson["inputVoltage"] = wf.inputVoltage;
            wfJson["outputVoltage"] = wf.outputVoltage;
            wfJson["inductorCurrent"] = wf.inductorCurrent;
            wfJson["operatingPointName"] = wf.operatingPointName;
            wfJson["dmAttenuation"] = wf.dmAttenuation;
            result.push_back(wfJson);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<size_t> get_only_temperature_dependent_indexes(std::string permeabilityPointsString) {
    try {
        std::vector<std::string> permeabilityPointsStringVector = json::parse(permeabilityPointsString);
        std::vector<PermeabilityPoint> permeabilityPoints;
        for (auto pointString : permeabilityPointsStringVector) {
            PermeabilityPoint point(json::parse(pointString));
            permeabilityPoints.push_back(point);
        }
        return OpenMagnetics::InitialPermeability::get_only_temperature_dependent_indexes(permeabilityPoints);
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::vector<size_t> get_only_frequency_dependent_indexes(std::string permeabilityPointsString) {
    try {
        std::vector<std::string> permeabilityPointsStringVector = json::parse(permeabilityPointsString);
        std::vector<PermeabilityPoint> permeabilityPoints;
        for (auto pointString : permeabilityPointsStringVector) {
            PermeabilityPoint point(json::parse(pointString));
            permeabilityPoints.push_back(point);
        }
        return OpenMagnetics::InitialPermeability::get_only_frequency_dependent_indexes(permeabilityPoints);
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::vector<size_t> get_only_magnetic_field_dc_bias_dependent_indexes(std::string permeabilityPointsString) {
    try {
        std::vector<std::string> permeabilityPointsStringVector = json::parse(permeabilityPointsString);
        std::vector<PermeabilityPoint> permeabilityPoints;
        for (auto pointString : permeabilityPointsStringVector) {
            PermeabilityPoint point(json::parse(pointString));
            permeabilityPoints.push_back(point);
        }
        return OpenMagnetics::InitialPermeability::get_only_magnetic_field_dc_bias_dependent_indexes(permeabilityPoints);
    }
    catch (const std::exception &exc) {
        std::cerr << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::string mas_autocomplete(std::string masString, bool simulate, std::string configurationString) {
    try {
        OpenMagnetics::Mas mas(json::parse(masString));
        json configuration(json::parse(configurationString));
        auto autocompletedMas = mas_autocomplete(mas, simulate, configuration);

        json result;
        to_json(result, autocompletedMas);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_steinmetz_coefficients(std::string dataString, std::string rangesString) {
    try {
        json rangesJson = json::parse(rangesString);
        std::vector<std::pair<double, double>> ranges;
        for (auto rangeJson : rangesJson) {
            std::pair<double, double> range{rangeJson[0], rangeJson[1]};
            ranges.push_back(range);
        }
        std::vector<VolumetricLossesPoint> data(json::parse(dataString));

        auto [coefficientsPerRange, errorPerRange] = OpenMagnetics::CoreLossesSteinmetzModel::calculate_steinmetz_coefficients(data, ranges);

        json result;
        to_json(result, coefficientsPerRange);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::map<std::string, std::string> get_initial_permeability_equations(std::string permeabilityPointString) {
    try {
        MAS::PermeabilityPoint permeabilityPoint(json::parse(permeabilityPointString));
        return OpenMagnetics::InitialPermeability::get_initial_permeability_equations(permeabilityPoint);
    }
    catch (const std::exception &exc) {
        return {{"Exception: ", std::string{exc.what()}}};
    }
}

std::map<std::string, std::string> get_core_volumetric_losses_equations(std::string coreLossesMethodDataString) {
    try {
        MAS::CoreLossesMethodData coreLossesMethodData(json::parse(coreLossesMethodDataString));
        return OpenMagnetics::CoreLossesProprietaryModel::get_core_volumetric_losses_equations(coreLossesMethodData);
    }
    catch (const std::exception &exc) {
        return {{"Exception: ", std::string{exc.what()}}};
    }
}


std::string calculate_complex_permeability(std::string coreMaterialString) {
    try {
        MAS::CoreMaterial coreMaterial(json::parse(coreMaterialString));

        ComplexPermeabilityData complexPermeabilityData;
        if (OpenMagnetics::InitialPermeability::has_frequency_dependency(coreMaterial)) {
            complexPermeabilityData = OpenMagnetics::ComplexPermeability().calculate_complex_permeability_from_frequency_dependent_initial_permeability(coreMaterial);
        }
        else {
            throw OpenMagnetics::MaterialDataMissingException("Missing complex data in material " + coreMaterial.get_name());
        }

        json result;
        to_json(result, complexPermeabilityData);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}


std::string plot_core(std::string magneticString) {
    try {
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_core(magnetic);
        painter.paint_bobbin(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string plot_sections(std::string magneticString) {
    try {
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_core(magnetic);
        painter.paint_bobbin(magnetic);
        painter.paint_coil_sections(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string plot_layers(std::string magneticString) {
    try {
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_core(magnetic);
        painter.paint_bobbin(magnetic);
        painter.paint_coil_layers(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string plot_turns(std::string magneticString) {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(true);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(false);
        std::filesystem::path emptyFilepath;
        
        auto magneticJson = json::parse(magneticString);
        
        OpenMagnetics::Magnetic magnetic(magneticJson);
        
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_core(magnetic);
        painter.paint_bobbin(magnetic);
        painter.paint_coil_turns(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}


std::string plot_magnetic_field(std::string magneticString, std::string operatingPointString) {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(true);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(false);
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        
        // For toroidal cores, ensure the coil is wound to generate additional_coordinates
        auto coil = magnetic.get_mutable_coil();
        auto core = magnetic.get_mutable_core();
        if (core.get_shape_family() == OpenMagnetics::CoreShapeFamily::T) {
            if (!coil.get_turns_description() || coil.get_turns_description()->empty()) {
                coil.wind();
                magnetic.set_coil(coil);
            }
        }

        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_magnetic_field(operatingPoint, magnetic);
        painter.paint_core(magnetic);
        // painter.paint_bobbin(magnetic);
        // Paint turns for H field
        painter.paint_coil_turns(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}


std::string plot_electric_field(std::string magneticString, std::string operatingPointString) {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(true);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(false);
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        
        // For toroidal cores, ensure the coil is wound to generate additional_coordinates
        auto coil = magnetic.get_mutable_coil();
        auto core = magnetic.get_mutable_core();
        if (core.get_shape_family() == OpenMagnetics::CoreShapeFamily::T) {
            if (!coil.get_turns_description() || coil.get_turns_description()->empty()) {
                coil.wind();
                magnetic.set_coil(coil);
            }
        }

        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_electric_field(operatingPoint, magnetic);
        painter.paint_core(magnetic);
        // painter.paint_bobbin(magnetic);
        // Paint turns for E field
        painter.paint_coil_turns(magnetic);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}


std::string plot_wire_losses(std::string magneticString, std::string operatingPointString) {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(true);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(false);
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_core(magnetic);
        painter.paint_bobbin(magnetic);
        // Paint turns with margins/layers
        painter.paint_coil_turns(magnetic);
        // Then paint wire losses on top
        try {
            painter.paint_wire_losses(magnetic, std::nullopt, operatingPoint);
        } catch (const std::exception& e) {
            // Wire losses already painted via coil_turns fallback
        }
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string plot_wire(std::string wireString) {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(false);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(true);
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Wire wire(json::parse(wireString));
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_wire(wire);
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string plot_temperature_field(std::string magneticString, std::string operatingPointString, std::string textColor = "#000000", std::string bgColor = "#FFFFFF") {
    try {
        OpenMagnetics::Settings::GetInstance().set_painter_simple_litz(true);
        OpenMagnetics::Settings::GetInstance().set_painter_advanced_litz(false);
        std::filesystem::path emptyFilepath;
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        OperatingPoint operatingPoint(json::parse(operatingPointString));
        
        // For toroidal cores, ensure the coil is wound
        auto coil = magnetic.get_mutable_coil();
        auto core = magnetic.get_mutable_core();
        if (core.get_shape_family() == OpenMagnetics::CoreShapeFamily::T) {
            if (!coil.get_turns_description() || coil.get_turns_description()->empty()) {
                coil.wind();
                magnetic.set_coil(coil);
            }
        }
        
        // Get ambient temperature from operating point
        double ambientTemperature = operatingPoint.get_conditions().get_ambient_temperature();
        
        // Run magnetic simulation to get losses
        OpenMagnetics::MagneticSimulator magneticSimulator;
        OpenMagnetics::Mas mas;
        mas.set_magnetic(magnetic);
        mas.get_mutable_inputs().set_operating_points({operatingPoint});  // Set the operating point for simulation
        auto simulatedMas = magneticSimulator.simulate(mas);
        
        double coreLosses = 0.0;
        double windingLosses = 0.0;
        std::optional<OpenMagnetics::WindingLossesOutput> windingLossesOutput;
        
        if (!simulatedMas.get_outputs().empty()) {
            auto outputs = simulatedMas.get_outputs()[0];
            if (outputs.get_core_losses().has_value()) {
                coreLosses = outputs.get_core_losses().value().get_core_losses();
            }
            if (outputs.get_winding_losses().has_value()) {
                windingLosses = outputs.get_winding_losses().value().get_winding_losses();
                // Also get the detailed per-turn losses (required for toroidal cores)
                windingLossesOutput = outputs.get_winding_losses().value();
            }
        }
        
        // Create temperature configuration
        OpenMagnetics::TemperatureConfig config;
        config.ambientTemperature = ambientTemperature;
        config.coreLosses = coreLosses;
        config.windingLosses = windingLosses;
        // Set per-turn losses (required for toroidal core thermal analysis)
        if (windingLossesOutput) {
            config.windingLossesOutput = windingLossesOutput;
        }
        
        // Create temperature model and calculate temperatures
        OpenMagnetics::Temperature temperature(magnetic, config);
        auto thermalResult = temperature.calculateTemperatures();
        
        // Use Painter class (same as magnetic field)
        OpenMagnetics::Painter painter(emptyFilepath, false, false, false);
        painter.paint_temperature_field(magnetic, thermalResult.nodeTemperatures, true, OpenMagnetics::ColorPalette::BLUE_TO_RED, ambientTemperature, textColor, bgColor);
        // Note: paint_core and paint_coil_turns are NOT called here because they would draw
        // the standard ferrite/copper geometry on top of the temperature visualization,
        // hiding the temperature colors. The temperature field function already draws
        // the core and turns with their temperature colors.
        
        auto result = painter.export_svg();
        return result;
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}


std::string set_interlayer_insulation(std::string coilString, double layerThickness){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        coil.set_interlayer_insulation(layerThickness);

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string set_intersection_insulation(std::string coilString, double layerThickness, int numberInsulationLayers){
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        coil.set_intersection_insulation(layerThickness, numberInsulationLayers);

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }
}

std::string calculate_filling_factor(std::string coilString) {
    try {
        OpenMagnetics::Coil coil(json::parse(coilString), false);
        auto [areaFillingFactor, aux] = coil.calculate_filling_factor();
        auto [overlappingFillingFactor, contiguousFillingFactor] = aux;
        json result;
        result["areaFillingFactor"] = areaFillingFactor;
        result["overlappingFillingFactor"] = overlappingFillingFactor;
        result["contiguousFillingFactor"] = contiguousFillingFactor;
        return result.dump(4);
    }
    catch(const std::runtime_error& re)
    {
        return re.what();
    }
    catch(const std::exception& ex)
    {
        return ex.what();
    }
    catch(...)
    {
        return "Unknown failure occurred. Possible memory corruption";
    }

}

std::string get_settings() {
    try {
        json settingsJson;

        settingsJson["magnetizingInductanceIncludeAirInductance"] = OpenMagnetics::Settings::GetInstance().get_magnetizing_inductance_include_air_inductance();
        settingsJson["coilAllowMarginTape"] = OpenMagnetics::Settings::GetInstance().get_coil_allow_margin_tape();
        settingsJson["coilAllowInsulatedWire"] = OpenMagnetics::Settings::GetInstance().get_coil_allow_insulated_wire();
        settingsJson["coilFillSectionsWithMarginTape"] = OpenMagnetics::Settings::GetInstance().get_coil_fill_sections_with_margin_tape();
        settingsJson["coilWindEvenIfNotFit"] = OpenMagnetics::Settings::GetInstance().get_coil_wind_even_if_not_fit();
        settingsJson["coilDelimitAndCompact"] = OpenMagnetics::Settings::GetInstance().get_coil_delimit_and_compact();
        settingsJson["coilOnlyOneTurnPerLayerInContiguousRectangular"] = OpenMagnetics::Settings::GetInstance().get_coil_only_one_turn_per_layer_in_contiguous_rectangular();
        settingsJson["coilTryRewind"] = OpenMagnetics::Settings::GetInstance().get_coil_try_rewind();
        settingsJson["coilMaximumLayersPlanar"] = OpenMagnetics::Settings::GetInstance().get_coil_maximum_layers_planar();
        settingsJson["coilIncludeAdditionalCoordinates"] = OpenMagnetics::Settings::GetInstance().get_coil_include_additional_coordinates();

        settingsJson["useOnlyCoresInStock"] = OpenMagnetics::Settings::GetInstance().get_use_only_cores_in_stock();
        settingsJson["painterNumberPointsX"] = OpenMagnetics::Settings::GetInstance().get_painter_number_points_x();
        settingsJson["painterNumberPointsY"] = OpenMagnetics::Settings::GetInstance().get_painter_number_points_y();
        settingsJson["painterMirroringDimension"] = OpenMagnetics::Settings::GetInstance().get_painter_mirroring_dimension();
        settingsJson["painterMode"] = OpenMagnetics::Settings::GetInstance().get_painter_mode();
        settingsJson["painterLogarithmicScale"] = OpenMagnetics::Settings::GetInstance().get_painter_logarithmic_scale();
        settingsJson["painterIncludeFringing"] = OpenMagnetics::Settings::GetInstance().get_painter_include_fringing();
        if (OpenMagnetics::Settings::GetInstance().get_painter_maximum_value_colorbar()) {
            settingsJson["painterMaximumValueColorbar"] = OpenMagnetics::Settings::GetInstance().get_painter_maximum_value_colorbar();
        }
        if (OpenMagnetics::Settings::GetInstance().get_painter_minimum_value_colorbar()) {
            settingsJson["painterMinimumValueColorbar"] = OpenMagnetics::Settings::GetInstance().get_painter_minimum_value_colorbar();
        }
        settingsJson["painterColorFerrite"] = OpenMagnetics::Settings::GetInstance().get_painter_color_ferrite();
        settingsJson["painterColorBobbin"] = OpenMagnetics::Settings::GetInstance().get_painter_color_bobbin();
        settingsJson["painterColorCopper"] = OpenMagnetics::Settings::GetInstance().get_painter_color_copper();
        settingsJson["painterColorInsulation"] = OpenMagnetics::Settings::GetInstance().get_painter_color_insulation();
        settingsJson["painterColorMargin"] = OpenMagnetics::Settings::GetInstance().get_painter_color_margin();
        settingsJson["magneticFieldNumberPointsX"] = OpenMagnetics::Settings::GetInstance().get_magnetic_field_number_points_x();
        settingsJson["magneticFieldNumberPointsY"] = OpenMagnetics::Settings::GetInstance().get_magnetic_field_number_points_y();
        settingsJson["magneticFieldMirroringDimension"] = OpenMagnetics::Settings::GetInstance().get_magnetic_field_mirroring_dimension();
        settingsJson["magneticFieldIncludeFringing"] = OpenMagnetics::Settings::GetInstance().get_magnetic_field_include_fringing();
        settingsJson["coilAdviserMaximumNumberWires"] = OpenMagnetics::Settings::GetInstance().get_coil_adviser_maximum_number_wires();
        settingsJson["coreIncludeMargin"] = OpenMagnetics::Settings::GetInstance().get_core_adviser_include_margin();
        settingsJson["coreIncludeStacks"] = OpenMagnetics::Settings::GetInstance().get_core_adviser_include_stacks();
        settingsJson["coreIncludeDistributedGaps"] = OpenMagnetics::Settings::GetInstance().get_core_adviser_include_distributed_gaps();
        settingsJson["verbose"] = OpenMagnetics::Settings::GetInstance().get_verbose();

        settingsJson["useToroidalCores"] = OpenMagnetics::Settings::GetInstance().get_use_toroidal_cores();
        settingsJson["useConcentricCores"] = OpenMagnetics::Settings::GetInstance().get_use_concentric_cores();

        // Model selection settings
        settingsJson["magneticFieldStrengthModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_magnetic_field_strength_model());
        settingsJson["magneticFieldStrengthFringingEffectModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_magnetic_field_strength_fringing_effect_model());
        settingsJson["reluctanceModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_reluctance_model());
        settingsJson["coreTemperatureModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_core_temperature_model());
        settingsJson["coreThermalResistanceModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_core_thermal_resistance_model());
        settingsJson["windingSkinEffectLossesModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_winding_skin_effect_losses_model());
        settingsJson["windingProximityEffectLossesModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_winding_proximity_effect_losses_model());
        settingsJson["strayCapacitanceModel"] = static_cast<int>(OpenMagnetics::Settings::GetInstance().get_stray_capacitance_model());
        settingsJson["coilEnableUserWindingLossesModels"] = OpenMagnetics::Settings::GetInstance().get_coil_enable_user_winding_losses_models();

        return settingsJson.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

void set_settings(std::string settingsString) {
    json settingsJson = json::parse(settingsString);

    OpenMagnetics::Settings::GetInstance().set_magnetizing_inductance_include_air_inductance(settingsJson["magnetizingInductanceIncludeAirInductance"]);
    OpenMagnetics::Settings::GetInstance().set_coil_allow_margin_tape(settingsJson["coilAllowMarginTape"]);
    OpenMagnetics::Settings::GetInstance().set_coil_allow_insulated_wire(settingsJson["coilAllowInsulatedWire"]);
    OpenMagnetics::Settings::GetInstance().set_coil_fill_sections_with_margin_tape(settingsJson["coilFillSectionsWithMarginTape"]);
    OpenMagnetics::Settings::GetInstance().set_coil_wind_even_if_not_fit(settingsJson["coilWindEvenIfNotFit"]);
    OpenMagnetics::Settings::GetInstance().set_coil_delimit_and_compact(settingsJson["coilDelimitAndCompact"]);
    OpenMagnetics::Settings::GetInstance().set_coil_only_one_turn_per_layer_in_contiguous_rectangular(settingsJson["coilOnlyOneTurnPerLayerInContiguousRectangular"]);
    OpenMagnetics::Settings::GetInstance().set_coil_try_rewind(settingsJson["coilTryRewind"]);
    OpenMagnetics::Settings::GetInstance().set_coil_maximum_layers_planar(settingsJson["coilMaximumLayersPlanar"]);
    if (settingsJson.contains("coilIncludeAdditionalCoordinates")) {
        OpenMagnetics::Settings::GetInstance().set_coil_include_additional_coordinates(settingsJson["coilIncludeAdditionalCoordinates"]);
    }

    OpenMagnetics::Settings::GetInstance().set_use_only_cores_in_stock(settingsJson["useOnlyCoresInStock"]);
    OpenMagnetics::Settings::GetInstance().set_painter_number_points_x(settingsJson["painterNumberPointsX"]);
    OpenMagnetics::Settings::GetInstance().set_painter_number_points_y(settingsJson["painterNumberPointsY"]);
    OpenMagnetics::Settings::GetInstance().set_painter_mirroring_dimension(settingsJson["painterMirroringDimension"]);
    OpenMagnetics::Settings::GetInstance().set_painter_mode(settingsJson["painterMode"]);
    OpenMagnetics::Settings::GetInstance().set_painter_logarithmic_scale(settingsJson["painterLogarithmicScale"]);
    OpenMagnetics::Settings::GetInstance().set_painter_include_fringing(settingsJson["painterIncludeFringing"]);
    if (settingsJson.contains("painterMaximumValueColorbar")) {
        OpenMagnetics::Settings::GetInstance().set_painter_maximum_value_colorbar(settingsJson["painterMaximumValueColorbar"]);
    }
    if (settingsJson.contains("painterMinimumValueColorbar")) {
        OpenMagnetics::Settings::GetInstance().set_painter_minimum_value_colorbar(settingsJson["painterMinimumValueColorbar"]);
    }
    OpenMagnetics::Settings::GetInstance().set_painter_color_ferrite(settingsJson["painterColorFerrite"]);
    OpenMagnetics::Settings::GetInstance().set_painter_color_bobbin(settingsJson["painterColorBobbin"]);
    OpenMagnetics::Settings::GetInstance().set_painter_color_copper(settingsJson["painterColorCopper"]);
    OpenMagnetics::Settings::GetInstance().set_painter_color_insulation(settingsJson["painterColorInsulation"]);
    OpenMagnetics::Settings::GetInstance().set_painter_color_margin(settingsJson["painterColorMargin"]);
    OpenMagnetics::Settings::GetInstance().set_magnetic_field_number_points_x(settingsJson["magneticFieldNumberPointsX"]);
    OpenMagnetics::Settings::GetInstance().set_magnetic_field_number_points_y(settingsJson["magneticFieldNumberPointsY"]);
    OpenMagnetics::Settings::GetInstance().set_magnetic_field_mirroring_dimension(settingsJson["magneticFieldMirroringDimension"]);
    OpenMagnetics::Settings::GetInstance().set_magnetic_field_include_fringing(settingsJson["magneticFieldIncludeFringing"]);
    OpenMagnetics::Settings::GetInstance().set_coil_adviser_maximum_number_wires(settingsJson["coilAdviserMaximumNumberWires"]);
    OpenMagnetics::Settings::GetInstance().set_core_adviser_include_margin(settingsJson["coreIncludeMargin"]);
    OpenMagnetics::Settings::GetInstance().set_core_adviser_include_stacks(settingsJson["coreIncludeStacks"]);
    OpenMagnetics::Settings::GetInstance().set_core_adviser_include_distributed_gaps(settingsJson["coreIncludeDistributedGaps"]);
    OpenMagnetics::Settings::GetInstance().set_verbose(settingsJson["verbose"]);

    OpenMagnetics::Settings::GetInstance().set_use_toroidal_cores(settingsJson["useToroidalCores"]);
    OpenMagnetics::Settings::GetInstance().set_use_concentric_cores(settingsJson["useConcentricCores"]);

    // Model selection settings
    if (settingsJson.contains("magneticFieldStrengthModel")) {
        OpenMagnetics::Settings::GetInstance().set_magnetic_field_strength_model(static_cast<OpenMagnetics::MagneticFieldStrengthModels>(settingsJson["magneticFieldStrengthModel"].get<int>()));
    }
    if (settingsJson.contains("magneticFieldStrengthFringingEffectModel")) {
        OpenMagnetics::Settings::GetInstance().set_magnetic_field_strength_fringing_effect_model(static_cast<OpenMagnetics::MagneticFieldStrengthFringingEffectModels>(settingsJson["magneticFieldStrengthFringingEffectModel"].get<int>()));
    }
    if (settingsJson.contains("reluctanceModel")) {
        OpenMagnetics::Settings::GetInstance().set_reluctance_model(static_cast<OpenMagnetics::ReluctanceModels>(settingsJson["reluctanceModel"].get<int>()));
    }
    if (settingsJson.contains("coreTemperatureModel")) {
        OpenMagnetics::Settings::GetInstance().set_core_temperature_model(static_cast<OpenMagnetics::CoreTemperatureModels>(settingsJson["coreTemperatureModel"].get<int>()));
    }
    if (settingsJson.contains("coreThermalResistanceModel")) {
        OpenMagnetics::Settings::GetInstance().set_core_thermal_resistance_model(static_cast<OpenMagnetics::CoreThermalResistanceModels>(settingsJson["coreThermalResistanceModel"].get<int>()));
    }
    if (settingsJson.contains("windingSkinEffectLossesModel")) {
        OpenMagnetics::Settings::GetInstance().set_winding_skin_effect_losses_model(static_cast<OpenMagnetics::WindingSkinEffectLossesModels>(settingsJson["windingSkinEffectLossesModel"].get<int>()));
    }
    if (settingsJson.contains("windingProximityEffectLossesModel")) {
        OpenMagnetics::Settings::GetInstance().set_winding_proximity_effect_losses_model(static_cast<OpenMagnetics::WindingProximityEffectLossesModels>(settingsJson["windingProximityEffectLossesModel"].get<int>()));
    }
    if (settingsJson.contains("strayCapacitanceModel")) {
        OpenMagnetics::Settings::GetInstance().set_stray_capacitance_model(static_cast<OpenMagnetics::StrayCapacitanceModels>(settingsJson["strayCapacitanceModel"].get<int>()));
    }
    if (settingsJson.contains("coilEnableUserWindingLossesModels")) {
        bool value = settingsJson["coilEnableUserWindingLossesModels"].get<bool>();
        OpenMagnetics::Settings::GetInstance().set_coil_enable_user_winding_losses_models(value);
    }

}
void reset_settings(std::string settingsString) {
    OpenMagnetics::Settings::GetInstance().reset();
}

std::string clear_magnetic_cache() {
    try {
        OpenMagnetics::magneticsCache.clear();
        return std::to_string(OpenMagnetics::magneticsCache.size());
    }
    catch (const std::exception &exc) {
        return std::string{exc.what()};
    }
}


std::string load_magnetic(std::string key, std::string magneticString, bool expand) {
    try {
        OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
        if (expand) {
            magnetic = OpenMagnetics::magnetic_autocomplete(magnetic);
        }
        OpenMagnetics::magneticsCache.load(key, magnetic);

        return std::to_string(OpenMagnetics::magneticsCache.size());
    }
    catch (const std::exception &exc) {
        return std::string{exc.what()};
    }
}

std::string load_magnetics(std::string keysString, std::string magneticsString, bool expand) {
    try {
        json keys = json::parse(keysString);
        json magneticJsons = json::parse(magneticsString);
        for (size_t magneticIndex = 0; magneticIndex < magneticJsons.size(); magneticIndex++) {
            OpenMagnetics::Magnetic magnetic(magneticJsons[magneticIndex]);
            if (expand) {
                magnetic = OpenMagnetics::magnetic_autocomplete(magnetic);
            }
            OpenMagnetics::magneticsCache.load(keys[magneticIndex], magnetic);
        }
        return std::to_string(OpenMagnetics::magneticsCache.size());
    }
    catch (const std::exception &exc) {
        return std::string{exc.what()};
    }
}

std::string load_magnetics_from_file(std::string path, bool expand) {
    try {
        std::ifstream in(path);
        if (in) {
            std::string line;
            while (getline(in, line)) {
                json jf = json::parse(line);
                OpenMagnetics::Magnetic magnetic(jf);
                if (expand) {
                    magnetic = OpenMagnetics::magnetic_autocomplete(magnetic);
                }
                std::string key = magnetic.get_manufacturer_info()->get_reference().value();
                OpenMagnetics::magneticsCache.load(key, magnetic);
            }
        }
        return std::to_string(OpenMagnetics::magneticsCache.size());
    }
    catch (const std::exception &exc) {
        return std::string{exc.what()};
    }
}

std::string load_magnetics_from_string(std::string database, bool expand) {
    try {
        std::string delimiter = "\n";
        size_t pos = 0;
        std::string token;
        if (database.back() != delimiter.back()) {
            database += delimiter;
        }
        while ((pos = database.find(delimiter)) != std::string::npos) {
            token = database.substr(0, pos);
            json jf = json::parse(token);
            OpenMagnetics::Magnetic magnetic(jf);
            OpenMagnetics::magneticsCache.load(jf["manufacturerInfo"]["reference"], magnetic);
            database.erase(0, pos + delimiter.length());
        }
        return std::to_string(OpenMagnetics::magneticsCache.size());
    }
    catch (const std::exception &exc) {
        return std::string{exc.what()};
    }
}

// Forward declarations for new topology functions
std::string calculate_llc_inputs(std::string llcInputsString);
std::string simulate_llc_ideal_waveforms(std::string llcInputsString);
std::string calculate_cllc_inputs(std::string cllcInputsString);
std::string calculate_dab_inputs(std::string dabInputsString);
std::string calculate_psfb_inputs(std::string psfbInputsString);

// SPICE Code Generation forward declarations
EMSCRIPTEN_KEEPALIVE std::string generate_flyback_ngspice_circuit(std::string flybackInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_buck_ngspice_circuit(std::string buckInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_boost_ngspice_circuit(std::string boostInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_push_pull_ngspice_circuit(std::string pushPullInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_two_switch_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_active_clamp_forward_ngspice_circuit(std::string forwardInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_isolated_buck_ngspice_circuit(std::string isolatedBuckInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_isolated_buck_boost_ngspice_circuit(std::string isolatedBuckBoostInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_llc_ngspice_circuit(std::string llcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_cllc_ngspice_circuit(std::string cllcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_dab_ngspice_circuit(std::string dabInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_psfb_ngspice_circuit(std::string psfbInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string generate_cmc_ngspice_circuit(std::string cmcInputsString, size_t inputVoltageIndex, size_t operatingPointIndex);
EMSCRIPTEN_KEEPALIVE std::string simulate_cmc_lisn_waveforms(std::string cmcInputsString, double inductance);
EMSCRIPTEN_KEEPALIVE std::string simulate_cmc_ideal_waveforms(std::string cmcInputsString, double inductance, double parasiticCap_pF, double dvdt_V_ns);

EMSCRIPTEN_BINDINGS(my_bindings) {
    function("get_constants", &get_constants);
    function("get_defaults", &get_defaults);
    function("standardize_signal_descriptor", &standardize_signal_descriptor);
    function("calculate_harmonics", &calculate_harmonics);
    function("get_main_harmonic_indexes", &get_main_harmonic_indexes);
    function("get_excitation_harmonic_indexes", &get_excitation_harmonic_indexes);
    function("calculate_processed", &calculate_processed);
    function("calculate_core_data", &calculate_core_data);
    function("calculate_bobbin_data", &calculate_bobbin_data);
    function("get_wire_data", &get_wire_data);
    function("get_wire_data_by_name", &get_wire_data_by_name);
    function("get_wire_data_by_standard_name", &get_wire_data_by_standard_name);
    function("get_strand_by_standard_name", &get_strand_by_standard_name);
    function("get_wire_outer_width_rectangular", &get_wire_outer_width_rectangular);
    function("get_wire_outer_height_rectangular", &get_wire_outer_height_rectangular);
    function("get_wire_outer_diameter_bare_litz", &get_wire_outer_diameter_bare_litz);
    function("get_wire_outer_diameter_served_litz", &get_wire_outer_diameter_served_litz);
    function("get_wire_outer_diameter_insulated_litz", &get_wire_outer_diameter_insulated_litz);
    function("get_wire_outer_diameter_enamelled_round", &get_wire_outer_diameter_enamelled_round);
    function("get_wire_outer_diameter_insulated_round", &get_wire_outer_diameter_insulated_round);
    function("get_wire_conducting_diameter_by_standard_name", &get_wire_conducting_diameter_by_standard_name);
    function("get_outer_dimensions", &get_outer_dimensions);
    function("get_equivalent_wire", &get_equivalent_wire);
    function("get_coating_label", &get_coating_label);
    function("get_wire_coating_by_label", &get_wire_coating_by_label);
    function("get_coating_labels_by_type", &get_coating_labels_by_type);
    function("load_core_data", &load_core_data);
    function("get_material_data", &get_material_data);
    function("get_core_temperature_dependant_parameters", &get_core_temperature_dependant_parameters);
    function("calculate_core_data_from_shape", &calculate_core_data_from_shape);
    function("calculate_all_core_data_from_shapes", &calculate_all_core_data_from_shapes);
    function("get_shape_data", &get_shape_data);
    function("get_available_core_materials", &get_available_core_materials);
    function("get_available_core_manufacturers", &get_available_core_manufacturers);
    function("get_available_core_shape_families", &get_available_core_shape_families);
    function("get_available_core_shapes", &get_available_core_shapes);
    function("get_available_core_shapes_by_manufacturer", &get_available_core_shapes_by_manufacturer);
    function("get_available_core_shapes_by_family", &get_available_core_shapes_by_family);
    function("get_shape_family_dimensions", &get_shape_family_dimensions);
    function("get_shape_family_subtypes", &get_shape_family_subtypes);
    function("get_available_wires", &get_available_wires);
    function("get_unique_wire_diameters", &get_unique_wire_diameters);
    function("get_planar_thicknesses", &get_planar_thicknesses);
    function("get_planar_wire_by_standard_name", &get_planar_wire_by_standard_name);
    function("get_available_wire_types", &get_available_wire_types);
    function("get_available_wire_standards", &get_available_wire_standards);
    function("calculate_gap_reluctance", &calculate_gap_reluctance);
    function("get_gap_reluctance_model_information", &get_gap_reluctance_model_information);
    function("calculate_inductance_from_number_turns_and_gapping", &calculate_inductance_from_number_turns_and_gapping);
    function("calculate_number_turns_from_gapping_and_inductance", &calculate_number_turns_from_gapping_and_inductance);
    function("calculate_gapping_from_number_turns_and_inductance", &calculate_gapping_from_number_turns_and_inductance);
    function("calculate_core_losses", &calculate_core_losses);
    function("get_core_losses_model_information", &get_core_losses_model_information);
    function("get_core_temperature_model_information", &get_core_temperature_model_information);
    function("resolve_dimension_with_tolerance", &resolve_dimension_with_tolerance);
    function("calculate_induced_voltage", &calculate_induced_voltage);
    function("calculate_induced_current", &calculate_induced_current);
    function("calculate_reflected_secondary", &calculate_reflected_secondary);
    function("calculate_reflected_primary", &calculate_reflected_primary);
    function("calculate_instantaneous_power", &calculate_instantaneous_power);
    function("calculate_rms_power", &calculate_rms_power);
    function("calculate_basic_processed_data", &calculate_basic_processed_data);
    function("create_waveform", &create_waveform);
    function("scale_waveform_time_to_frequency", &scale_waveform_time_to_frequency);
    function("scale_excitation_time_to_frequency", &scale_excitation_time_to_frequency);
    function("calculate_insulation", &calculate_insulation);
    function("extract_operating_point", &extract_operating_point);
    function("extract_map_column_names", &extract_map_column_names);
    function("extract_column_names", &extract_column_names);
    function("calculate_number_turns", &calculate_number_turns);
    function("calculate_dc_resistance_per_meter", &calculate_dc_resistance_per_meter);
    function("calculate_dc_resistance_per_winding", &calculate_dc_resistance_per_winding);
    function("calculate_dc_losses_per_meter", &calculate_dc_losses_per_meter);
    function("calculate_skin_ac_losses_per_meter", &calculate_skin_ac_losses_per_meter);
    function("calculate_skin_ac_factor", &calculate_skin_ac_factor);
    function("calculate_skin_ac_resistance_per_meter", &calculate_skin_ac_resistance_per_meter);
    function("calculate_effective_current_density", &calculate_effective_current_density);
    function("calculate_effective_skin_depth", &calculate_effective_skin_depth);
    function("get_available_winding_orientations", &get_available_winding_orientations);
    function("get_available_coil_alignments", &get_available_coil_alignments);
    function("check_requirement", &check_requirement);
    function("wind", &wind);
    function("wind_planar", &wind_planar);
    function("wind_by_sections", &wind_by_sections);
    function("wind_by_layers", &wind_by_layers);
    function("wind_by_turns", &wind_by_turns);
    function("delimit_and_compact", &delimit_and_compact);
    function("get_layers_by_winding_index", &get_layers_by_winding_index);
    function("get_layers_by_section", &get_layers_by_section);
    function("get_sections_description_conduction", &get_sections_description_conduction);
    function("simulate", &simulate);
    function("are_sections_and_layers_fitting", &are_sections_and_layers_fitting);
    function("add_margin_to_section_by_index", &add_margin_to_section_by_index);
    function("check_if_fits", &check_if_fits);
    function("export_magnetic_as_subcircuit", &export_magnetic_as_subcircuit);
    function("export_magnetic_as_symbol", &export_magnetic_as_symbol);
    function("calculate_ac_resistance_coefficients_per_winding", &calculate_ac_resistance_coefficients_per_winding);
    function("sweep_impedance_over_frequency", &sweep_impedance_over_frequency);
    function("sweep_q_factor_over_frequency", &sweep_q_factor_over_frequency);
    function("sweep_winding_resistance_over_frequency", &sweep_winding_resistance_over_frequency);
    function("sweep_resistance_over_frequency", &sweep_resistance_over_frequency);
    function("sweep_core_losses_over_frequency", &sweep_core_losses_over_frequency);
    function("sweep_winding_losses_over_frequency", &sweep_winding_losses_over_frequency);
    function("sweep_magnetizing_inductance_over_frequency", &sweep_magnetizing_inductance_over_frequency);
    function("sweep_magnetizing_inductance_over_temperature", &sweep_magnetizing_inductance_over_temperature);
    function("sweep_magnetizing_inductance_over_dc_bias", &sweep_magnetizing_inductance_over_dc_bias);
    function("load_core_materials", &load_core_materials);
    function("load_core_shapes", &load_core_shapes);
    function("load_wires", &load_wires);
    function("clear_databases", &clear_databases);
    function("is_core_material_database_empty", &is_core_material_database_empty);
    function("is_core_shape_database_empty", &is_core_shape_database_empty);
    function("is_wire_database_empty", &is_wire_database_empty);
    function("get_maximum_dimensions", &get_maximum_dimensions);
    function("calculate_advised_cores", &calculate_advised_cores);
    function("calculate_advised_sections", &calculate_advised_sections);
    function("calculate_advised_coil", &calculate_advised_coil);
    function("calculate_advised_wires", &calculate_advised_wires);
    function("get_solid_insulation_requirements_for_wires", &get_solid_insulation_requirements_for_wires);
    function("calculate_advised_magnetics", &calculate_advised_magnetics);
    function("calculate_advised_magnetics_from_catalog", &calculate_advised_magnetics_from_catalog);
    function("calculate_advised_magnetics_from_cache", &calculate_advised_magnetics_from_cache);
    function("get_available_core_filters", &get_available_core_filters);
    function("load_cores", &load_cores);
    function("clear_loaded_cores", &clear_loaded_cores);
    function("calculate_leakage_inductance", &calculate_leakage_inductance);
    function("calculate_inductance_matrix", &calculate_inductance_matrix);
    function("calculate_coupling_coefficient_matrix", &calculate_coupling_coefficient_matrix);
    function("calculate_leakage_inductance_matrix", &calculate_leakage_inductance_matrix);
    function("calculate_stray_capacitance", &calculate_stray_capacitance);
    function("calculate_capacitance_matrix", &calculate_capacitance_matrix);
    function("calculate_maxwell_capacitance_matrix", &calculate_maxwell_capacitance_matrix);
    function("calculate_capacitance_models_between_windings", &calculate_capacitance_models_between_windings);
    function("get_available_core_losses_methods", &get_available_core_losses_methods);
    function("get_all_magnetic_field_strength_models", &get_all_magnetic_field_strength_models);
    function("get_all_fringing_effect_models", &get_all_fringing_effect_models);
    function("get_all_reluctance_models", &get_all_reluctance_models);
    function("get_all_winding_skin_effect_models", &get_all_winding_skin_effect_models);
    function("get_all_winding_proximity_effect_models", &get_all_winding_proximity_effect_models);
    function("get_all_stray_capacitance_models", &get_all_stray_capacitance_models);
    function("calculate_resistance_matrix", &calculate_resistance_matrix);
    function("calculate_flyback_inputs", &calculate_flyback_inputs);
    function("calculate_advanced_flyback_inputs", &calculate_advanced_flyback_inputs);
    function("simulate_flyback_ideal_waveforms", &simulate_flyback_ideal_waveforms);
    function("simulate_flyback_with_magnetic", &simulate_flyback_with_magnetic);
    function("calculate_isolated_buck_inputs", &calculate_isolated_buck_inputs);
    function("calculate_advanced_isolated_buck_inputs", &calculate_advanced_isolated_buck_inputs);
    function("calculate_isolated_buck_boost_inputs", &calculate_isolated_buck_boost_inputs);
    function("calculate_advanced_isolated_buck_boost_inputs", &calculate_advanced_isolated_buck_boost_inputs);
    function("simulate_isolated_buck_boost_ideal_waveforms", &simulate_isolated_buck_boost_ideal_waveforms);
    function("simulate_isolated_buck_ideal_waveforms", &simulate_isolated_buck_ideal_waveforms);
    function("calculate_buck_inputs", &calculate_buck_inputs);
    function("calculate_advanced_buck_inputs", &calculate_advanced_buck_inputs);
    function("simulate_buck_ideal_waveforms", &simulate_buck_ideal_waveforms);
    function("calculate_boost_inputs", &calculate_boost_inputs);
    function("calculate_advanced_boost_inputs", &calculate_advanced_boost_inputs);
    function("simulate_boost_ideal_waveforms", &simulate_boost_ideal_waveforms);
    function("calculate_push_pull_inputs", &calculate_push_pull_inputs);
    function("calculate_advanced_push_pull_inputs", &calculate_advanced_push_pull_inputs);
    function("simulate_push_pull_ideal_waveforms", &simulate_push_pull_ideal_waveforms);
    // Forward converter functions
    function("calculate_single_switch_forward_inputs", &calculate_single_switch_forward_inputs);
    function("calculate_advanced_single_switch_forward_inputs", &calculate_advanced_single_switch_forward_inputs);
    function("simulate_forward_ideal_waveforms", &simulate_forward_ideal_waveforms);
    function("calculate_active_clamp_forward_inputs", &calculate_active_clamp_forward_inputs);
    function("calculate_advanced_active_clamp_forward_inputs", &calculate_advanced_active_clamp_forward_inputs);
    function("simulate_active_clamp_forward_ideal_waveforms", &simulate_active_clamp_forward_ideal_waveforms);
    function("calculate_two_switch_forward_inputs", &calculate_two_switch_forward_inputs);
    function("calculate_advanced_two_switch_forward_inputs", &calculate_advanced_two_switch_forward_inputs);
    function("simulate_two_switch_forward_ideal_waveforms", &simulate_two_switch_forward_ideal_waveforms);
    
    // SPICE Code Generation functions
    function("generate_flyback_ngspice_circuit", &generate_flyback_ngspice_circuit);
    function("generate_buck_ngspice_circuit", &generate_buck_ngspice_circuit);
    function("generate_boost_ngspice_circuit", &generate_boost_ngspice_circuit);
    function("generate_push_pull_ngspice_circuit", &generate_push_pull_ngspice_circuit);
    function("generate_forward_ngspice_circuit", &generate_forward_ngspice_circuit);
    function("generate_two_switch_forward_ngspice_circuit", &generate_two_switch_forward_ngspice_circuit);
    function("generate_active_clamp_forward_ngspice_circuit", &generate_active_clamp_forward_ngspice_circuit);
    function("generate_isolated_buck_ngspice_circuit", &generate_isolated_buck_ngspice_circuit);
    function("generate_isolated_buck_boost_ngspice_circuit", &generate_isolated_buck_boost_ngspice_circuit);
    function("generate_llc_ngspice_circuit", &generate_llc_ngspice_circuit);
    // function("generate_cllc_ngspice_circuit", &generate_cllc_ngspice_circuit);  // Different signature - requires CllcResonantParameters
    function("generate_dab_ngspice_circuit", &generate_dab_ngspice_circuit);
    function("generate_psfb_ngspice_circuit", &generate_psfb_ngspice_circuit);
    function("generate_cmc_ngspice_circuit", &generate_cmc_ngspice_circuit);

    function("calculate_pfc_inputs", &calculate_pfc_inputs);
    function("simulate_pfc_waveforms", &simulate_pfc_waveforms);
    function("determine_pfc_mode", &determine_pfc_mode);
    function("calculate_cmc_inputs", &calculate_cmc_inputs);
    function("calculate_advanced_cmc_inputs", &calculate_advanced_cmc_inputs);
    function("simulate_cmc_lisn_waveforms", &simulate_cmc_lisn_waveforms);
    function("simulate_cmc_ideal_waveforms", &simulate_cmc_ideal_waveforms);
    function("calculate_dmc_inputs", &calculate_dmc_inputs);
    function("verify_dmc_attenuation", &verify_dmc_attenuation);
    function("propose_dmc_design", &propose_dmc_design);
    function("simulate_dmc_waveforms", &simulate_dmc_waveforms);
    function("get_only_temperature_dependent_indexes", &get_only_temperature_dependent_indexes);
    function("get_only_frequency_dependent_indexes", &get_only_frequency_dependent_indexes);
    function("get_only_magnetic_field_dc_bias_dependent_indexes", &get_only_magnetic_field_dc_bias_dependent_indexes);
    function("create_simple_bobbin_from_core", &create_simple_bobbin_from_core);
    function("create_simple_bobbin_from_core_with_custom_thickness", &create_simple_bobbin_from_core_with_custom_thickness);
    function("create_simple_bobbin_from_core_with_custom_thicknesses", &create_simple_bobbin_from_core_with_custom_thicknesses);
    function("mas_autocomplete", &mas_autocomplete);
    function("calculate_steinmetz_coefficients", &calculate_steinmetz_coefficients);
    function("get_initial_permeability_equations", &get_initial_permeability_equations);
    function("get_core_volumetric_losses_equations", &get_core_volumetric_losses_equations);
    function("calculate_complex_permeability", &calculate_complex_permeability);
    function("plot_core", &plot_core);
    function("plot_sections", &plot_sections);
    function("plot_layers", &plot_layers);
    function("plot_turns", &plot_turns);
    function("plot_magnetic_field", &plot_magnetic_field);
    function("plot_electric_field", &plot_electric_field);
    function("plot_temperature_field", &plot_temperature_field);
    function("plot_wire_losses", &plot_wire_losses);
    function("plot_wire", &plot_wire);
    function("set_interlayer_insulation", &set_interlayer_insulation);
    function("set_intersection_insulation", &set_intersection_insulation);
    function("calculate_filling_factor", &calculate_filling_factor);
    function("get_settings", &get_settings);
    function("set_settings", &set_settings);
    function("reset_settings", &reset_settings);
    function("clear_magnetic_cache", &clear_magnetic_cache);
    function("load_magnetic", &load_magnetic);
    function("load_magnetics", &load_magnetics);
    function("load_magnetics_from_file", &load_magnetics_from_file);
    function("load_magnetics_from_string", &load_magnetics_from_string);
    
    // New topology wizard functions
    function("calculate_llc_inputs", &calculate_llc_inputs);
    function("simulate_llc_ideal_waveforms", &simulate_llc_ideal_waveforms);
    function("calculate_cllc_inputs", &calculate_cllc_inputs);
    function("calculate_dab_inputs", &calculate_dab_inputs);
    function("calculate_psfb_inputs", &calculate_psfb_inputs);
    
    // New integrated converter processing functions
    function("process_converter", &process_converter);
    function("design_magnetics_from_converter", &design_magnetics_from_converter);
    
    register_map<std::string, double>("map<string, double>");
    register_map<std::string, std::string>("map<string, string>");
    // register_map<std::string, std::map<std::string, std::string>>("map<string, map<string, string>>");
    register_vector<std::string>("vector<std::string>");
    register_vector<int>("vector<int>");
    register_vector<double>("vector<double>");
    register_vector<size_t>("vector<size_t>");

};

// New topology functions - DAB, LLC, CLLC, PSFB

std::string calculate_dab_inputs(std::string dabInputsString) {
    try {
        json dabInputsJson = json::parse(dabInputsString);
        
        OpenMagnetics::Dab dabInputs(dabInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (dabInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = dabInputsJson["numberOfPeriods"].get<size_t>();
        }
        dabInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = dabInputs.process();
        
        json result;
        to_json(result, inputs);
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

EMSCRIPTEN_KEEPALIVE std::string calculate_llc_inputs(std::string llcInputsString){
    try {
        json llcInputsJson = json::parse(llcInputsString);
        
        OpenMagnetics::Llc llcInputs(llcInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (llcInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = llcInputsJson["numberOfPeriods"].get<size_t>();
        }
        llcInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto inputs = llcInputs.process();
        
        json result;
        to_json(result, inputs);
        
        // Debug: Log excitation names
        std::cerr << "DEBUG calculate_llc_inputs: " << result["operatingPoints"].size() << " operating points" << std::endl;
        for (size_t i = 0; i < result["operatingPoints"].size(); ++i) {
            const auto& op = result["operatingPoints"][i];
            std::cerr << "  OP " << i << ": " << op["excitationsPerWinding"].size() << " excitations" << std::endl;
            for (size_t j = 0; j < op["excitationsPerWinding"].size(); ++j) {
                const auto& exc = op["excitationsPerWinding"][j];
                std::string name = exc.contains("name") && !exc["name"].is_null() ? exc["name"].get<std::string>() : "NULL";
                std::cerr << "    Excitation " << j << ": name='" << name << "'" << std::endl;
            }
        }
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string simulate_llc_ideal_waveforms(std::string llcInputsString) {
    try {
        json llcInputsJson = json::parse(llcInputsString);

        // Check for multi-output request (not yet supported for LLC)
        if (llcInputsJson.contains("operatingPoints") && llcInputsJson["operatingPoints"].is_array()) {
            for (const auto& op : llcInputsJson["operatingPoints"]) {
                if (op.contains("outputVoltages") && op["outputVoltages"].is_array() && op["outputVoltages"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for LLC converter.");
                }
                if (op.contains("outputCurrents") && op["outputCurrents"].is_array() && op["outputCurrents"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for LLC converter.");
                }
            }
        }

        OpenMagnetics::Llc llcInputs(llcInputsJson);

        auto designRequirements = llcInputs.process_design_requirements();
        double magnetizingInductance = llcInputsJson.value("magnetizingInductance", 200e-6);

        // Get turns ratios from design requirements
        std::vector<double> turnsRatios;
        for (const auto& tr : designRequirements.get_turns_ratios()) {
            if (tr.get_nominal()) {
                turnsRatios.push_back(tr.get_nominal().value());
            }
        }

#ifndef ENABLE_NGSPICE
        throw std::runtime_error("ngspice simulation is required but ENABLE_NGSPICE was not defined at compile time");
#endif

        OpenMagnetics::NgspiceRunner runner;
        if (!runner.is_available()) {
            throw std::runtime_error("ngspice simulation is required but ngspice is not available");
        }

        // Read number of periods from input (default to 2)
        size_t numberOfPeriods = 2;
        if (llcInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = llcInputsJson["numberOfPeriods"].get<size_t>();
        }
        
        // Read number of steady state periods from input (default to 3)
        size_t numberOfSteadyStatePeriods = 3;
        if (llcInputsJson.contains("numberOfSteadyStatePeriods")) {
            numberOfSteadyStatePeriods = llcInputsJson["numberOfSteadyStatePeriods"].get<size_t>();
        }
        
        // Set the number of periods for the LLC simulation
        llcInputs.set_num_periods_to_extract(static_cast<int>(numberOfPeriods));
        llcInputs.set_num_steady_state_periods(static_cast<int>(numberOfSteadyStatePeriods));
        
        // Run both simulations (topology waveforms + operating points)
        auto topologyWaveforms = llcInputs.simulate_and_extract_topology_waveforms(
            turnsRatios, magnetizingInductance, numberOfPeriods);
        auto operatingPoints = llcInputs.simulate_and_extract_operating_points(
            turnsRatios, magnetizingInductance);

        // DEBUG: Check if data is preserved after return
        std::cerr << "DEBUG libMKF LLC: Returned " << operatingPoints.size() << " operating points" << std::endl;
        for (size_t i = 0; i < operatingPoints.size(); ++i) {
            const auto& op = operatingPoints[i];
            std::cerr << "  OP " << i << ": " << op.get_excitations_per_winding().size() << " excitations" << std::endl;
            for (size_t j = 0; j < op.get_excitations_per_winding().size(); ++j) {
                const auto& exc = op.get_excitations_per_winding()[j];
                std::cerr << "    Exc " << j << ":";
                if (exc.get_voltage() && exc.get_voltage()->get_waveform()) {
                    std::cerr << " V=" << exc.get_voltage()->get_waveform()->get_data().size();
                } else {
                    std::cerr << " V=none";
                }
                if (exc.get_current() && exc.get_current()->get_waveform()) {
                    std::cerr << " I=" << exc.get_current()->get_waveform()->get_data().size();
                } else {
                    std::cerr << " I=none";
                }
                std::cerr << std::endl;
            }
        }

        // Build the result with inputs and converterWaveforms
        json result;

        // inputs: OpenMagnetics::Inputs containing designRequirements and operatingPoints
        json inputsJson;
        inputsJson["designRequirements"] = json();
        to_json(inputsJson["designRequirements"], designRequirements);
        inputsJson["operatingPoints"] = json::array();
        for (const auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            inputsJson["operatingPoints"].push_back(opJson);
        }
        result["inputs"] = inputsJson;

        // DEBUG: Check data after JSON building
        std::cerr << "DEBUG libMKF LLC after JSON: " << operatingPoints.size() << " OPs" << std::endl;
        for (size_t i = 0; i < operatingPoints.size() && i < 1; ++i) {
            const auto& op = operatingPoints[i];
            std::cerr << "  OP " << i << ": " << op.get_excitations_per_winding().size() << " excs" << std::endl;
            for (size_t j = 0; j < op.get_excitations_per_winding().size() && j < 1; ++j) {
                const auto& exc = op.get_excitations_per_winding()[j];
                if (exc.get_voltage() && exc.get_voltage()->get_waveform()) {
                    std::cerr << "    V=" << exc.get_voltage()->get_waveform()->get_data().size();
                } else {
                    std::cerr << "    V=none";
                }
                std::cerr << std::endl;
            }
        }

        // converterWaveforms: array of OpenMagnetics::ConverterWaveforms
        result["converterWaveforms"] = json::array();
        for (const auto& tw : topologyWaveforms) {
            json cwJson;
            to_json(cwJson, tw);
            result["converterWaveforms"].push_back(cwJson);
        }
        
        // DEBUG: Check converterWaveforms after JSON serialization
        std::cerr << "DEBUG libMKF LLC converterWaveforms JSON:" << std::endl;
        for (size_t i = 0; i < result["converterWaveforms"].size(); ++i) {
            const auto& cw = result["converterWaveforms"][i];
            std::cerr << "  CW " << i << ":";
            if (cw.contains("inputVoltage") && cw["inputVoltage"].contains("data")) {
                std::cerr << " Vin=" << cw["inputVoltage"]["data"].size();
            } else {
                std::cerr << " Vin=none";
            }
            if (cw.contains("inputCurrent") && cw["inputCurrent"].contains("data")) {
                std::cerr << " Iin=" << cw["inputCurrent"]["data"].size();
            } else {
                std::cerr << " Iin=none";
            }
            std::cerr << std::endl;
        }

        return result.dump(4);
    }
    catch (const std::exception& exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

std::string calculate_cllc_inputs(std::string cllcInputsString) {
    try {
        json cllcInputsJson = json::parse(cllcInputsString);
        
        // Check for multi-output request (not yet supported for CLLC)
        if (cllcInputsJson.contains("operatingPoints") && cllcInputsJson["operatingPoints"].is_array()) {
            for (const auto& op : cllcInputsJson["operatingPoints"]) {
                if (op.contains("outputVoltages") && op["outputVoltages"].is_array() && op["outputVoltages"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for CLLC converter. Please use a single output (outputVoltages array with one element).");
                }
                if (op.contains("outputCurrents") && op["outputCurrents"].is_array() && op["outputCurrents"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for CLLC converter. Please use a single output (outputCurrents array with one element).");
                }
            }
        }
        
        OpenMagnetics::CllcConverter cllcInputs(cllcInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (cllcInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = cllcInputsJson["numberOfPeriods"].get<size_t>();
        }
        cllcInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto designRequirements = cllcInputs.process_design_requirements();
        
        double magnetizingInductance = cllcInputsJson.value("magnetizingInductance", 100e-6);
        
        auto operatingPoints = cllcInputs.process_operating_points({}, magnetizingInductance);
        
        // Commented out due to missing calculate_resonant_tank method
        // if (!cllcInputsJson.contains("primaryResonantInductance")) {
        //     double ls1, cs1, ls2, cs2, lm;
        //     double fr = (cllcInputs.get_min_switching_frequency() + cllcInputs.get_max_switching_frequency()) / 2.0;
        //     cllcInputs.calculate_resonant_tank(fr, cllcInputs.get_quality_factor().value_or(0.4), ls1, cs1, ls2, cs2, lm);
        // }
        
        json result;
        to_json(result, designRequirements);
        result["operatingPoints"] = json::array();
        
        for (auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            result["operatingPoints"].push_back(opJson);
        }
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

std::string calculate_psfb_inputs(std::string psfbInputsString) {
    try {
        json psfbInputsJson = json::parse(psfbInputsString);
        
        // Check for multi-output request (not yet supported for PSFB)
        if (psfbInputsJson.contains("operatingPoints") && psfbInputsJson["operatingPoints"].is_array()) {
            for (const auto& op : psfbInputsJson["operatingPoints"]) {
                if (op.contains("outputVoltages") && op["outputVoltages"].is_array() && op["outputVoltages"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for PSFB converter. Please use a single output (outputVoltages array with one element).");
                }
                if (op.contains("outputCurrents") && op["outputCurrents"].is_array() && op["outputCurrents"].size() > 1) {
                    throw std::runtime_error("Multi-output configuration is not yet supported for PSFB converter. Please use a single output (outputCurrents array with one element).");
                }
            }
        }
        
        OpenMagnetics::Psfb psfbInputs(psfbInputsJson);
        
        // Read number of periods from input (default to 1 for analytical)
        size_t numberOfPeriods = 1;
        if (psfbInputsJson.contains("numberOfPeriods")) {
            numberOfPeriods = psfbInputsJson["numberOfPeriods"].get<size_t>();
        }
        psfbInputs.set_num_periods_to_extract(numberOfPeriods);
        
        auto designRequirements = psfbInputs.process_design_requirements();
        
        double magnetizingInductance = psfbInputsJson.value("magnetizingInductance", 1e-3);
        
        auto operatingPoints = psfbInputs.process_operating_points({}, magnetizingInductance);
        
        json result;
        to_json(result, designRequirements);
        result["operatingPoints"] = json::array();
        
        for (auto& op : operatingPoints) {
            json opJson;
            to_json(opJson, op);
            result["operatingPoints"].push_back(opJson);
        }
        
        // Repeat waveforms for the specified number of periods (analytical generates 1 period)
        if (numberOfPeriods > 1 && result.contains("operatingPoints")) {
            repeat_operating_points_waveforms(result["operatingPoints"], numberOfPeriods);
        }
        
        result["masInputs"] = json::object();
        to_json(result["masInputs"], designRequirements);
        if (!result["operatingPoints"].empty()) {
            result["masInputs"]["operatingPoints"] = json::array({result["operatingPoints"][0]});
        } else {
            result["masInputs"]["operatingPoints"] = json::array();
        }
        
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

// New integrated functions for topology processing and magnetic advising

std::string process_converter(std::string topologyName, std::string converterJson, bool useNgspice) {
    try {
        json converterData = json::parse(converterJson);
        json result;
        
        // Normalize topology name to lowercase
        std::string topology = topologyName;
        std::transform(topology.begin(), topology.end(), topology.begin(), ::tolower);
        
        if (topology == "flyback" || topology == "advanced_flyback") {
            bool isAdvanced = converterData.contains("desiredInductance");
            if (useNgspice) {
                // simulate_flyback_ideal_waveforms handles both regular and advanced internally
                return simulate_flyback_ideal_waveforms(converterJson);
            } else {
                if (isAdvanced) {
                    return calculate_advanced_flyback_inputs(converterJson);
                } else {
                    return calculate_flyback_inputs(converterJson);
                }
            }
        }
        else if (topology == "buck" || topology == "advanced_buck") {
            if (useNgspice) {
                return simulate_buck_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_buck_inputs(converterJson);
                } else {
                    return calculate_buck_inputs(converterJson);
                }
            }
        }
        else if (topology == "boost" || topology == "advanced_boost") {
            if (useNgspice) {
                return simulate_boost_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_boost_inputs(converterJson);
                } else {
                    return calculate_boost_inputs(converterJson);
                }
            }
        }
        else if (topology == "isolated_buck" || topology == "advanced_isolated_buck") {
            if (useNgspice) {
                return simulate_isolated_buck_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_isolated_buck_inputs(converterJson);
                } else {
                    return calculate_isolated_buck_inputs(converterJson);
                }
            }
        }
        else if (topology == "isolated_buck_boost" || topology == "advanced_isolated_buck_boost") {
            if (useNgspice) {
                return simulate_isolated_buck_boost_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_isolated_buck_boost_inputs(converterJson);
                } else {
                    return calculate_isolated_buck_boost_inputs(converterJson);
                }
            }
        }
        else if (topology == "push_pull" || topology == "advanced_push_pull") {
            if (useNgspice) {
                return simulate_push_pull_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_push_pull_inputs(converterJson);
                } else {
                    return calculate_push_pull_inputs(converterJson);
                }
            }
        }
        else if (topology == "single_switch_forward" || topology == "advanced_single_switch_forward") {
            if (useNgspice) {
                return simulate_forward_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_single_switch_forward_inputs(converterJson);
                } else {
                    return calculate_single_switch_forward_inputs(converterJson);
                }
            }
        }
        else if (topology == "two_switch_forward" || topology == "advanced_two_switch_forward") {
            if (useNgspice) {
                return simulate_two_switch_forward_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_two_switch_forward_inputs(converterJson);
                } else {
                    return calculate_two_switch_forward_inputs(converterJson);
                }
            }
        }
        else if (topology == "active_clamp_forward" || topology == "advanced_active_clamp_forward") {
            if (useNgspice) {
                return simulate_active_clamp_forward_ideal_waveforms(converterJson);
            } else {
                if (converterData.contains("desiredInductance")) {
                    return calculate_advanced_active_clamp_forward_inputs(converterJson);
                } else {
                    return calculate_active_clamp_forward_inputs(converterJson);
                }
            }
        }
        else if (topology == "llc" || topology == "advanced_llc") {
            if (useNgspice) {
                return simulate_llc_ideal_waveforms(converterJson);
            } else {
                return calculate_llc_inputs(converterJson);
            }
        }
        else if (topology == "cllc" || topology == "advanced_cllc") {
            return calculate_cllc_inputs(converterJson);
        }
        else if (topology == "dab" || topology == "advanced_dab") {
            return calculate_dab_inputs(converterJson);
        }
        else if (topology == "psfb" || topology == "phase_shifted_full_bridge" || 
                 topology == "advanced_psfb" || topology == "advanced_phase_shifted_full_bridge") {
            return calculate_psfb_inputs(converterJson);
        }
        else {
            json error;
            error["error"] = "Unknown topology: " + topologyName;
            return error.dump(4);
        }
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}

std::string design_magnetics_from_converter(std::string topologyName, std::string converterJson, 
                                             int maxResults, std::string coreModeString, 
                                             bool useNgspice, std::string weightsString) {
    try {
        // Step 1: Process the converter to get Inputs
        std::string inputsResult = process_converter(topologyName, converterJson, useNgspice);
        json inputsJson = json::parse(inputsResult);
        
        if (inputsJson.contains("error")) {
            return inputsResult;  // Return the error
        }
        
        // Step 2: Process inputs to ensure they're properly formatted
        OpenMagnetics::Inputs inputs(inputsJson, true);
        
        // Step 3: Set up magnetic adviser
        OpenMagnetics::MagneticAdviser magneticAdviser;
        
        // Parse core mode
        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        OpenMagnetics::from_json(coreModeString, coreMode);
        magneticAdviser.set_core_mode(coreMode);
        
        // Parse weights if provided
        std::map<OpenMagnetics::MagneticFilters, double> weights;
        if (!weightsString.empty() && weightsString != "{}") {
            std::map<std::string, double> weightsKeysString = json::parse(weightsString);
            double externalSum = 0;
            for (const auto& pair : weightsKeysString) {
                externalSum += pair.second;
            }
            for (const auto& [filterName, weight] : weightsKeysString) {
                OpenMagnetics::MagneticFilters filter;
                OpenMagnetics::from_json(filterName, filter);
                weights[filter] = weight / externalSum;
            }
        }
        
        // Step 4: Get advised magnetics
        std::vector<std::pair<OpenMagnetics::Mas, double>> masMagnetics;
        if (weights.empty()) {
            masMagnetics = magneticAdviser.get_advised_magnetic(inputs, maxResults);
        } else {
            masMagnetics = magneticAdviser.get_advised_magnetic(inputs, weights, maxResults);
        }
        
        // Step 5: Build result
        auto scorings = magneticAdviser.get_scorings();
        json results;
        results["data"] = json::array();
        
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            std::string name = masMagnetic.get_magnetic().get_manufacturer_info().value().get_reference().value();
            
            json result;
            json masJson;
            to_json(masJson, masMagnetic);
            result["mas"] = masJson;
            result["scoring"] = scoring;
            
            // Add scoring per filter if available
            if (scorings.count(name)) {
                result["scoringPerFilter"] = json();
                for (size_t index = 0; index < magic_enum::enum_count<OpenMagnetics::MagneticFilters>(); ++index) {
                    auto filter = static_cast<OpenMagnetics::MagneticFilters>(index);
                    auto filterString = OpenMagnetics::to_string(filter);
                    if (scorings[name].count(filter)) {
                        result["scoringPerFilter"][filterString] = scorings[name][filter];
                    }
                }
            }
            
            results["data"].push_back(result);
        }
        
        // Sort by scoring
        std::sort(results["data"].begin(), results["data"].end(), 
                  [](json& b1, json& b2) {
                      return b1["scoring"] > b2["scoring"];
                  });
        
        return results.dump(4);
    }
    catch (const std::exception &exc) {
        json error;
        error["error"] = std::string{exc.what()};
        return error.dump(4);
    }
}
