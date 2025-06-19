#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "Constants.h"
#include "constructive_models/Insulation.h"
#include "Defaults.h"
#include <MAS.hpp>
#include "constructive_models/Coil.h"
#include "constructive_models/NumberTurns.h"
#include "constructive_models/CorePiece.h"
#include "processors/MagneticSimulator.h"
#include "physical_models/WindingOhmicLosses.h"
#include "physical_models/WindingSkinEffectLosses.h"
#include "advisers/WireAdviser.h"
#include "advisers/CoilAdviser.h"
#include "advisers/CoreAdviser.h"
#include "advisers/MagneticAdviser.h"
#include "processors/Inputs.h"
#include "constructive_models/Core.h"
#include "physical_models/Reluctance.h"
#include "physical_models/LeakageInductance.h"
#include "physical_models/MagnetizingInductance.h"
#include "converter_models/Topology.h"
#include "physical_models/CoreLosses.h"
#include "physical_models/CoreTemperature.h"
#include "support/Utils.h"
#include "processors/Sweeper.h"
#include "processors/CircuitSimulatorInterface.h"


using namespace MAS;
using namespace emscripten;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

std::map<std::string, double> get_constants() {
    std::map<std::string, double> constantsMap;
    constantsMap["residualGap"] = constants.residualGap;
    constantsMap["minimumNonResidualGap"] = constants.minimumNonResidualGap;
    constantsMap["vacuumPermeability"] = constants.vacuumPermeability;
    constantsMap["vacuumPermittivity"] = constants.vacuumPermittivity;
    constantsMap["quasiStaticFrequencyLimit"] = constants.quasiStaticFrequencyLimit;
    constantsMap["spacerProtudingPercentage"] = constants.spacerProtudingPercentage;
    constantsMap["coilPainterScale"] = constants.coilPainterScale;
    constantsMap["numberPointsSampledWaveforms"] = constants.numberPointsSampledWaveforms;
    constantsMap["minimumDistributedFringingFactor"] = constants.minimumDistributedFringingFactor;
    constantsMap["maximumDistributedFringingFactor"] = constants.maximumDistributedFringingFactor;
    constantsMap["initialGapLengthForSearching"] = constants.initialGapLengthForSearching;
    constantsMap["roshenMagneticFieldStrengthStep"] = constants.roshenMagneticFieldStrengthStep;
    constantsMap["foilToSectionMargin"] = constants.foilToSectionMargin;
    constantsMap["planarToSectionMargin"] = constants.planarToSectionMargin;
    return constantsMap;
}

std::map<std::string, double> get_defaults() {
    std::map<std::string, double> defaultsMap;
    defaultsMap["maximumProportionMagneticFluxDensitySaturation"] = defaults.maximumProportionMagneticFluxDensitySaturation;
    defaultsMap["coreAdviserFrequencyReference"] = defaults.coreAdviserFrequencyReference;
    defaultsMap["coreAdviserMagneticFluxDensityReference"] = defaults.coreAdviserMagneticFluxDensityReference;
    defaultsMap["coreAdviserThresholdValidity"] = defaults.coreAdviserThresholdValidity;
    defaultsMap["coreAdviserMaximumCoreTemperature"] = defaults.coreAdviserMaximumCoreTemperature;
    defaultsMap["coreAdviserMaximumPercentagePowerCoreLosses"] = defaults.coreAdviserMaximumPercentagePowerCoreLosses;
    defaultsMap["coreAdviserMaximumMagneticsAfterFiltering"] = defaults.coreAdviserMaximumMagneticsAfterFiltering;
    defaultsMap["coreAdviserMaximumNumberStacks"] = defaults.coreAdviserMaximumNumberStacks;
    defaultsMap["maximumCurrentDensity"] = defaults.maximumCurrentDensity;
    defaultsMap["maximumEffectiveCurrentDensity"] = defaults.maximumEffectiveCurrentDensity;
    defaultsMap["maximumNumberParallels"] = defaults.maximumNumberParallels;
    defaultsMap["magneticFluxDensitySaturation"] = defaults.magneticFluxDensitySaturation;
    defaultsMap["magnetizingInductanceThresholdValidity"] = defaults.magnetizingInductanceThresholdValidity;
    defaultsMap["harmonicAmplitudeThreshold"] = defaults.harmonicAmplitudeThreshold;
    defaultsMap["ambientTemperature"] = defaults.ambientTemperature;
    defaultsMap["measurementFrequency"] = defaults.measurementFrequency;
    defaultsMap["magneticFieldMirroringDimension"] = defaults.magneticFieldMirroringDimension;
    defaultsMap["maximumCoilPattern"] = defaults.maximumCoilPattern;
    defaultsMap["overlappingFactorSurroundingTurns"] = defaults.overlappingFactorSurroundingTurns;

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
        std::cout << std::string{exc.what()} << std::endl;
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
        std::cout << std::string{exc.what()} << std::endl;
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
        std::cout << harmonicsString << std::endl;
        std::cout << waveformString << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_shape_data(std::string shapeString){
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
                bobbin = OpenMagnetics::Bobbin::create_quick_bobbin(magnetic.get_mutable_core());
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

std::string get_wire_data(std::string coilFunctionalDescriptionDataString){
    OpenMagnetics::CoilFunctionalDescription coilFunctionalDescription(json::parse(coilFunctionalDescriptionDataString));
    auto wire = OpenMagnetics::Coil::resolve_wire(coilFunctionalDescription);
    json result;
    to_json(result, wire);
    return result.dump(4);
}

std::string get_wire_data_by_name(std::string name){
    auto wireData = OpenMagnetics::find_wire_by_name(name);
    json result;
    to_json(result, wireData);
    return result.dump(4);
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

    for (auto wire : wires) {
        if (!wire.get_standard_name()) {
            continue;
        }
        if (wire.get_standard_name().value() == standardName) {
            auto coating = wire.resolve_coating();
            if (!coating) {
                continue;
            }
            // if (!coating->get_grade()) {
            //     continue;
            // }
            // // Hardcoded
            // if (coating->get_grade().value() == 1) {
                json result;
                to_json(result, wire);
                return result.dump(4);
            // }
        }
    }

    json result;
    result["errorMessage"] = "Wire not found by standard name";
    return result.dump(4);
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
        std::cout << std::string{exc.what()} << std::endl;

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
    json result = json::array();
    for (auto& coreJson : json::parse(coresString)) {
        OpenMagnetics::Core core(coreJson, false);
        json aux;
        to_json(aux, core);
        result.push_back(aux);
    }
    return result.dump(4);
}

std::string get_material_data(std::string materialName){

    auto materialData = OpenMagnetics::find_core_material_by_name(materialName);
    json result;
    to_json(result, materialData);
    return result.dump(4);
}

std::string get_core_temperature_dependant_parameters(std::string coreData, double temperature){
    OpenMagnetics::Core core(json::parse(coreData));
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
    for (auto& family : OpenMagnetics::get_shape_families()) {
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
    return OpenMagnetics::get_material_names(manufacturer);
}

std::vector<std::string> get_available_core_shapes(){
    return OpenMagnetics::get_shape_names();
}

std::vector<std::string> get_available_core_shapes_by_manufacturer(std::string manufacturer){
    return OpenMagnetics::get_core_shapes_names(manufacturer);
}

std::vector<std::string> get_available_core_shapes_by_family(std::string familyString){
    try {
        CoreShapeFamily family;
        from_json(familyString, family);
         
        return OpenMagnetics::get_shape_names(family);
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
    for (auto [reference, wire] : wireDatabase) {
        auto wireType = wire.get_type();
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
    for (auto [reference, wire] : wireDatabase) {
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
        std::string modelNameStringUpper = modelNameString;
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        auto modelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper);

        auto reluctanceModel = OpenMagnetics::ReluctanceModel::factory(modelName.value());
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
    json info;
    info["information"] = OpenMagnetics::ReluctanceModel::get_models_information();
    info["errors"] = OpenMagnetics::ReluctanceModel::get_models_errors();
    info["internal_links"] = OpenMagnetics::ReluctanceModel::get_models_internal_links();
    info["external_links"] = OpenMagnetics::ReluctanceModel::get_models_external_links();
    return info.dump(4);
}

double calculate_inductance_from_number_turns_and_gapping(std::string coreData,
                                                          std::string coilData,
                                                          std::string operatingPointData,
                                                          std::string modelsData){
    try {
        OpenMagnetics::Core core(json::parse(coreData));
        OpenMagnetics::Coil coil(json::parse(coilData), false);

        std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

        auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            std::string modelNameStringUpper = models["reluctance"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
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
        std::cout << coreData << std::endl;
        std::cout << coilData << std::endl;
        std::cout << operatingPointData << std::endl;
        std::cout << modelsData << std::endl;
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
        return -1;
    }
}


double calculate_number_turns_from_gapping_and_inductance(std::string coreData,
                                                          std::string inputsData,    
                                                          std::string modelsData){
    OpenMagnetics::Core core(json::parse(coreData));
    OpenMagnetics::Inputs inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    
    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
    double numberTurns = magnetizingInductanceObj.calculate_number_turns_from_gapping_and_inductance(core, &inputs);

    return numberTurns;
}


std::string calculate_gapping_from_number_turns_and_inductance(std::string coreData,
                                                               std::string coilData,
                                                               std::string inputsData,
                                                               std::string gappingTypeString,
                                                               int decimals,
                                                               std::string modelsData){
    OpenMagnetics::Core core(json::parse(coreData));
    OpenMagnetics::Coil coil(json::parse(coilData), false);
    OpenMagnetics::Inputs inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    OpenMagnetics::GappingType gappingType = magic_enum::enum_cast<OpenMagnetics::GappingType>(gappingTypeString).value();
    
    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
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

std::string calculate_core_losses(std::string coreData,
                                  std::string coilData,
                                  std::string inputsData,    
                                  std::string modelsData,    
                                  int operatingPointIndex){
 
    try {

        OpenMagnetics::Core core(json::parse(coreData));
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

        auto reluctanceModelName = defaults.reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            std::string modelNameStringUpper = models["reluctance"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
        }
        auto coreLossesModelName = defaults.coreLossesModelDefault;
        if (models.find("coreLosses") != models.end()) {
            std::string modelNameStringUpper = models["coreLosses"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            coreLossesModelName = magic_enum::enum_cast<OpenMagnetics::CoreLossesModels>(modelNameStringUpper).value();
        }
        auto coreTemperatureModelName = defaults.coreTemperatureModelDefault;
        if (models.find("coreTemperature") != models.end()) {
            std::string modelNameStringUpper = models["coreTemperature"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            coreTemperatureModelName = magic_enum::enum_cast<OpenMagnetics::CoreTemperatureModels>(modelNameStringUpper).value();
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
        return "Exception: " + std::string{exc.what()};
    }
}

std::string get_core_losses_model_information(std::string material){
    json info;
    info["information"] = OpenMagnetics::CoreLossesModel::get_models_information();
    info["errors"] = OpenMagnetics::CoreLossesModel::get_models_errors();
    info["internal_links"] = OpenMagnetics::CoreLossesModel::get_models_internal_links();
    info["external_links"] = OpenMagnetics::CoreLossesModel::get_models_external_links();
    info["available_models"] = OpenMagnetics::CoreLossesModel::get_methods_string(material);
    return info.dump(4);
}

std::string get_core_temperature_model_information(){
    json info;
    info["information"] = OpenMagnetics::CoreTemperatureModel::get_models_information();
    info["errors"] = OpenMagnetics::CoreTemperatureModel::get_models_errors();
    info["internal_links"] = OpenMagnetics::CoreTemperatureModel::get_models_internal_links();
    info["external_links"] = OpenMagnetics::CoreTemperatureModel::get_models_external_links();
    return info.dump(4);
}

std::string calculate_induced_voltage(std::string excitationString, double magnetizingInductance){
    OperatingPointExcitation excitation(json::parse(excitationString));

    auto voltage = OpenMagnetics::Inputs::calculate_induced_voltage(excitation, magnetizingInductance);

    json result;
    to_json(result, voltage);
    return result.dump(4);
}

std::string calculate_induced_current(std::string excitationString, double magnetizingInductance){
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

std::string calculate_reflected_secondary(std::string primaryExcitationString, double turnRatio){
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

std::string calculate_reflected_primary(std::string secondaryExcitationString, double turnRatio){
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
    OperatingPointExcitation excitation(json::parse(excitationString));

    auto voltageSignalDescriptor = excitation.get_voltage().value();
    auto currentSignalDescriptor = excitation.get_current().value();

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

    double rmsPower = currentSignalDescriptor.get_processed().value().get_rms().value() * voltageSignalDescriptor.get_processed().value().get_rms().value();

    return rmsPower;
}

double resolve_dimension_with_tolerance(std::string dimensionWithToleranceString) {
    DimensionWithTolerance dimensionWithTolerance(json::parse(dimensionWithToleranceString));
    return OpenMagnetics::resolve_dimensional_values(dimensionWithTolerance);
}

std::string calculate_basic_processed_data(std::string waveformString) {
    Waveform waveform(json::parse(waveformString));
    auto processed = OpenMagnetics::Inputs::calculate_basic_processed_data(waveform);
    json result;
    to_json(result, processed);
    return result.dump(4);
}

std::string create_waveform(std::string processedString, double frequency) {
    Processed processed(json::parse(processedString));
    auto waveform = OpenMagnetics::Inputs::create_waveform(processed, frequency);
    json result;
    to_json(result, waveform);
    return result.dump(4);
}

std::string scale_waveform_time_to_frequency(std::string waveformString, double newFrequency) {
    Waveform waveform(json::parse(waveformString));
    auto scaledWaveform = OpenMagnetics::Inputs::scale_time_to_frequency(waveform, newFrequency);
    json result;
    to_json(result, scaledWaveform);
    return result.dump(4);
}

std::string scale_excitation_time_to_frequency(std::string excitationString, double newFrequency) {
    OperatingPointExcitation excitation(json::parse(excitationString));
    OpenMagnetics::Inputs::scale_time_to_frequency(excitation, newFrequency, false, true);
    json result;
    to_json(result, excitation);
    return result.dump(4);
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

std::string extract_column_names(std::string fileString){
    auto reader = OpenMagnetics::CircuitSimulationReader(fileString, true);
    auto columnNames = reader.extract_column_names();

    json result = json::array();
    for (auto& columnName : columnNames) {
        result.push_back(columnName);
    }
    return result.dump(4);
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
    for (auto& [orientation, _] : magic_enum::enum_entries<WindingOrientation>()) {
        json orientationJson;
        to_json(orientationJson, orientation);
        orientations.push_back(orientationJson);
    }
    return orientations;
}

std::vector<std::string> get_available_coil_alignments(){
    std::vector<std::string> orientations;
    for (auto& [orientation, _] : magic_enum::enum_entries<CoilAlignment>()) {
        json orientationJson;
        to_json(orientationJson, orientation);
        orientations.push_back(orientationJson);
    }
    return orientations;
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

std::string wind(std::string coilString, size_t repetitions, std::string proportionPerWindingString, std::string patternString, std::string marginPairsString) {
    try {
        auto coilJson = json::parse(coilString);
        auto marginPairs = std::vector<std::vector<double>>(json::parse(marginPairsString));

        std::vector<double> proportionPerWinding = json::parse(proportionPerWindingString);
        std::vector<size_t> pattern = json::parse(patternString);
        auto coilFunctionalDescription = std::vector<OpenMagnetics::CoilFunctionalDescription>(coilJson["functionalDescription"]);
        OpenMagnetics::Coil coil;
        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
        coil.preload_margins(marginPairs);

        if (coilJson["_layersOrientation"].is_object()) {
            auto layersOrientationPerSection = std::map<std::string, WindingOrientation>(coilJson["_layersOrientation"]);
            for (auto [sectionName, layerOrientation] : layersOrientationPerSection) {
                coil.set_layers_orientation(layerOrientation, sectionName);
            }
        }
        else if (coilJson["_layersOrientation"].is_array()) {
            coil.wind_by_sections(proportionPerWinding, pattern, repetitions);
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto layersOrientationPerSection = std::vector<WindingOrientation>(coilJson["_layersOrientation"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < layersOrientationPerSection.size()) {
                        coil.set_layers_orientation(layersOrientationPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            WindingOrientation layerOrientation(coilJson["_layersOrientation"]);
            coil.set_layers_orientation(layerOrientation);

        }
        if (coilJson["_turnsAlignment"].is_object()) {
            auto turnsAlignmentPerSection = std::map<std::string, CoilAlignment>(coilJson["_turnsAlignment"]);
            for (auto [sectionName, turnsAlignment] : turnsAlignmentPerSection) {
                coil.set_turns_alignment(turnsAlignment, sectionName);
            }
        }
        else if (coilJson["_turnsAlignment"].is_array()) {
            coil.wind_by_sections(proportionPerWinding, pattern, repetitions);
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto turnsAlignmentPerSection = std::vector<CoilAlignment>(coilJson["_turnsAlignment"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < turnsAlignmentPerSection.size()) {
                        coil.set_turns_alignment(turnsAlignmentPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            CoilAlignment turnsAlignment(coilJson["_turnsAlignment"]);
            coil.set_turns_alignment(turnsAlignment);
        }

        if (proportionPerWinding.size() == coilFunctionalDescription.size()) {
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

        json result;
        to_json(result, coil);
        return result.dump(4);
    }
    catch (const std::exception &exc) {
        std::cout << coilString << std::endl;
        std::cout << repetitions << std::endl;
        std::cout << proportionPerWindingString << std::endl;
        std::cout << patternString << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::string wind_by_sections(std::string coilString, size_t repetitions, std::string proportionPerWindingString, std::string patternString) {
    try {
        auto coilJson = json::parse(coilString);

        std::vector<double> proportionPerWinding = json::parse(proportionPerWindingString);
        std::vector<size_t> pattern = json::parse(patternString);
        auto coilFunctionalDescription = std::vector<OpenMagnetics::CoilFunctionalDescription>(coilJson["functionalDescription"]);
        OpenMagnetics::Coil coil;

        if (coilJson.contains("_interleavingLevel")) {
            coil.set_interleaving_level(coilJson["_interleavingLevel"]);
        }
        if (coilJson.contains("_windingOrientation")) {
            coil.set_winding_orientation(coilJson["_windingOrientation"]);
        }
        if (coilJson.contains("_layersOrientation")) {
            coil.set_layers_orientation(coilJson["_layersOrientation"]);
        }
        if (coilJson.contains("_turnsAlignment")) {
            coil.set_turns_alignment(coilJson["_turnsAlignment"]);
        }
        if (coilJson.contains("_sectionAlignment")) {
            coil.set_section_alignment(coilJson["_sectionAlignment"]);
        }

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
        if (proportionPerWinding.size() == coilFunctionalDescription.size()) {
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

        auto coilFunctionalDescription = std::vector<OpenMagnetics::CoilFunctionalDescription>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        OpenMagnetics::Coil coil;

        if (coilJson.contains("_interleavingLevel")) {
            coil.set_interleaving_level(coilJson["_interleavingLevel"]);
        }
        if (coilJson.contains("_windingOrientation")) {
            coil.set_winding_orientation(coilJson["_windingOrientation"]);
        }
        if (coilJson.contains("_layersOrientation")) {
            coil.set_layers_orientation(coilJson["_layersOrientation"]);
        }
        if (coilJson.contains("_turnsAlignment")) {
            coil.set_turns_alignment(coilJson["_turnsAlignment"]);
        }
        if (coilJson.contains("_sectionAlignment")) {
            coil.set_section_alignment(coilJson["_sectionAlignment"]);
        }

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
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

        auto coilFunctionalDescription = std::vector<OpenMagnetics::CoilFunctionalDescription>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<Layer>(coilJson["layersDescription"]);
        OpenMagnetics::Coil coil;

        if (coilJson.contains("_interleavingLevel")) {
            coil.set_interleaving_level(coilJson["_interleavingLevel"]);
        }
        if (coilJson.contains("_windingOrientation")) {
            coil.set_winding_orientation(coilJson["_windingOrientation"]);
        }
        if (coilJson.contains("_layersOrientation")) {
            coil.set_layers_orientation(coilJson["_layersOrientation"]);
        }
        if (coilJson.contains("_turnsAlignment")) {
            coil.set_turns_alignment(coilJson["_turnsAlignment"]);
        }
        if (coilJson.contains("_sectionAlignment")) {
            coil.set_section_alignment(coilJson["_sectionAlignment"]);
        }

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
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

        auto coilFunctionalDescription = std::vector<OpenMagnetics::CoilFunctionalDescription>(coilJson["functionalDescription"]);
        auto coilSectionsDescription = std::vector<Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<Layer>(coilJson["layersDescription"]);
        auto coilTurnsDescription = std::vector<Turn>(coilJson["turnsDescription"]);
        OpenMagnetics::Coil coil;

        if (coilJson.contains("_interleavingLevel")) {
            coil.set_interleaving_level(coilJson["_interleavingLevel"]);
        }
        if (coilJson.contains("_windingOrientation")) {
            coil.set_winding_orientation(coilJson["_windingOrientation"]);
        }
        if (coilJson.contains("_layersOrientation")) {
            coil.set_layers_orientation(coilJson["_layersOrientation"]);
        }
        if (coilJson.contains("_turnsAlignment")) {
            coil.set_turns_alignment(coilJson["_turnsAlignment"]);
        }
        if (coilJson.contains("_sectionAlignment")) {
            coil.set_section_alignment(coilJson["_sectionAlignment"]);
        }

        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
        coil.set_sections_description(coilSectionsDescription);
        coil.set_layers_description(coilLayersDescription);
        coil.set_turns_description(coilTurnsDescription);
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
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
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

        auto reluctanceModelName = defaults.reluctanceModelDefault;
        if (models.find("reluctance") != models.end()) {
            std::string modelNameStringUpper = models["reluctance"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
        }
        auto coreLossesModelName = defaults.coreLossesModelDefault;
        if (models.find("coreLosses") != models.end()) {
            std::string modelNameStringUpper = models["coreLosses"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            coreLossesModelName = magic_enum::enum_cast<OpenMagnetics::CoreLossesModels>(modelNameStringUpper).value();
        }
        auto coreTemperatureModelName = defaults.coreTemperatureModelDefault;
        if (models.find("coreTemperature") != models.end()) {
            std::string modelNameStringUpper = models["coreTemperature"];
            std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
            coreTemperatureModelName = magic_enum::enum_cast<OpenMagnetics::CoreTemperatureModels>(modelNameStringUpper).value();
        }

        OpenMagnetics::MagneticSimulator magneticSimulator;

        magneticSimulator.set_core_losses_model_name(coreLossesModelName);
        magneticSimulator.set_core_temperature_model_name(coreTemperatureModelName);
        magneticSimulator.set_reluctance_model_name(reluctanceModelName);
        auto mas = magneticSimulator.simulate(inputs, magnetic);

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
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
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
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
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

        return coreMaterialDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cout << std::string{exc.what()} << std::endl;
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
        return coreShapeDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cout << std::string{exc.what()} << std::endl;
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
        return wireDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cout << std::string{exc.what()} << std::endl;
        return -1;
    }
}

size_t load_cores(std::string fileToLoad, bool includeToroids, bool useOnlyCoresInStock){
    try {
        if (fileToLoad != "") {
            OpenMagnetics::load_cores(fileToLoad);
        }
        else {
            settings->set_use_toroidal_cores(includeToroids);
            settings->set_use_only_cores_in_stock(useOnlyCoresInStock);
            OpenMagnetics::load_cores();
        }
        return coreDatabase.size();
    }
    catch (const std::exception &exc) {
        std::cout << std::string{exc.what()} << std::endl;
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
    return coreMaterialDatabase.size() == 0;
}

bool is_core_shape_database_empty(){
    return coreShapeDatabase.size() == 0;
}

bool is_wire_database_empty(){
    return wireDatabase.size() == 0;
}

std::vector<double> get_maximum_dimensions(std::string magneticString){
    OpenMagnetics::Magnetic magnetic(json::parse(magneticString));
    return magnetic.get_maximum_dimensions();
}

std::string calculate_advised_cores(std::string inputsString, std::string weightsString, int maximumNumberResults, bool useOnlyCoresInStock){
    try {
        settings->set_coil_delimit_and_compact(true);

        OpenMagnetics::Inputs inputs(json::parse(inputsString));
        std::map<std::string, double> weightsKeysString = json::parse(weightsString);
        std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;

        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::AREA_PRODUCT] = 1;
        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::ENERGY_STORED] = 1;

        bool filterMode = bool(inputs.get_design_requirements().get_minimum_impedance());

        if (filterMode) {
            settings->set_use_toroidal_cores(true);
            settings->set_use_only_cores_in_stock(false);
            settings->set_use_concentric_cores(false);
        }

        double externalSum = 0;
        for (auto const& pair : weightsKeysString) {
            externalSum += pair.second;
        }

        for (auto const& pair : weightsKeysString) {
            weights[magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(pair.first).value()] = pair.second / externalSum;
        }
        if (filterMode) {
            weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::MINIMUM_IMPEDANCE] = weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::EFFICIENCY];
            weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::EFFICIENCY] = 0;
        }

        OpenMagnetics::CoreAdviser coreAdviser;
        auto masMagnetics = coreAdviser.get_advised_core(inputs, weights, maximumNumberResults);
        auto log = coreAdviser.read_log();
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
                    auto magnetizingInductanceOutputEnergy = mas.get_mutable_outputs()[operatingPointIndex].get_magnetizing_inductance();
                    magnetizingInductanceOutput.set_maximum_magnetic_energy_core(magnetizingInductanceOutputEnergy->get_maximum_magnetic_energy_core());
                    mas.get_mutable_outputs()[operatingPointIndex].set_magnetizing_inductance(magnetizingInductanceOutput);
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
            for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreAdviser::CoreAdviserFilters>()) {
                std::string filterString(filter);
                result["scoringPerFilter"][filterString] = scoring[name][magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(filterString).value()];
            };
            result["scoringPerFilter"].erase(magic_enum::enum_name(OpenMagnetics::CoreAdviser::CoreAdviserFilters::AREA_PRODUCT));
            result["scoringPerFilter"].erase(magic_enum::enum_name(OpenMagnetics::CoreAdviser::CoreAdviserFilters::ENERGY_STORED));
            results["data"].push_back(result);
        }
        results["log"] = log;

        return results.dump(4);
    }
    catch (const std::exception &exc) {
        std::cout << inputsString << std::endl;
        std::cout << weightsString << std::endl;
        std::cout << maximumNumberResults << std::endl;
        std::cout << useOnlyCoresInStock << std::endl;
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
        settings->set_coil_delimit_and_compact(true);
        OpenMagnetics::Mas mas(json::parse(masString));

        for (size_t windingIndex = 0; windingIndex < mas.get_magnetic().get_coil().get_functional_description().size(); ++windingIndex) {
            mas.get_mutable_magnetic().get_mutable_coil().get_mutable_functional_description()[windingIndex].set_wire("Dummy");
        }
        mas.get_mutable_magnetic().get_mutable_coil().set_turns_description(std::nullopt);
        mas.get_mutable_magnetic().get_mutable_coil().set_layers_description(std::nullopt);
        mas.get_mutable_magnetic().get_mutable_coil().set_sections_description(std::nullopt);

        OpenMagnetics::CoilAdviser coilAdviser;
        auto masMagneticsWithCoil = coilAdviser.get_advised_coil(mas, 1);

        if (masMagneticsWithCoil.size() > 0) {
            json result = json();
            to_json(result, masMagneticsWithCoil[0]);
            return result.dump(4);
        }
        else{
            return "Exception: No coil found";
        }
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

std::string calculate_advised_wires(std::string coilFunctionalDescriptionString,
                                    std::string sectionString,
                                    std::string currentString,
                                    std::string solidInsulationRequirementsString,
                                    double temperature,
                                    uint8_t numberSections,
                                    size_t maximumNumberResults){
    try {
        settings->set_coil_delimit_and_compact(true);
        OpenMagnetics::CoilFunctionalDescription coilFunctionalDescription(json::parse(coilFunctionalDescriptionString));
        OpenMagnetics::WireSolidInsulationRequirements wireSolidInsulationRequirements(json::parse(solidInsulationRequirementsString));
        Section section(json::parse(sectionString));
        SignalDescriptor current(json::parse(currentString));

        OpenMagnetics::WireAdviser wireAdviser;
        wireAdviser.set_wire_solid_insulation_requirements(wireSolidInsulationRequirements);
        auto coilFunctionalDescriptions = wireAdviser.get_advised_wire(coilFunctionalDescription, section, current, temperature, numberSections, maximumNumberResults);

        json results = json();
        results["data"] = json::array();
        for (auto& [coilFunctionalDescription, scoring] : coilFunctionalDescriptions) {
            json result;
            json coilFunctionalDescriptionJson;
            to_json(coilFunctionalDescriptionJson, coilFunctionalDescription);
            result["coilFunctionalDescription"] = coilFunctionalDescriptionJson;
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

std::string calculate_advised_magnetics(std::string inputsString, std::string weightsString, int maximumNumberResults, bool useOnlyCoresInStock){
    settings->set_coil_delimit_and_compact(true);
    OpenMagnetics::Inputs inputs(json::parse(inputsString));
    std::map<std::string, double> weightsKeysString = json::parse(weightsString);
    std::map<OpenMagnetics::MagneticFilters, double> weights;

    double externalSum = 0;
    for (auto const& pair : weightsKeysString) {
        externalSum += pair.second;
    }

    for (auto const& pair : weightsKeysString) {
        weights[magic_enum::enum_cast<OpenMagnetics::MagneticFilters>(pair.first).value()] = pair.second / externalSum;
    }

    settings->set_use_only_cores_in_stock(useOnlyCoresInStock);

    OpenMagnetics::MagneticAdviser magneticAdviser;
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
        for (auto& filter : magic_enum::enum_names<OpenMagnetics::MagneticFilters>()) {
            std::string filterString(filter);
            auto filterEnum = magic_enum::enum_cast<OpenMagnetics::MagneticFilters>(filterString).value();
            if (scorings[name].count(filterEnum)) {
                result["scoringPerFilter"][filterString] = scorings[name][filterEnum];
            }
        };
        results["data"].push_back(result);
    }

    sort(results["data"].begin(), results["data"].end(), [](json& b1, json& b2) {
        return b1["weightedTotalScoring"] > b2["weightedTotalScoring"];
    });

    return results.dump(4);
}

std::string calculate_advised_magnetics_from_catalog(std::string inputsString, std::string catalogString, int maximumNumberResults){
    try {
        settings->set_coil_delimit_and_compact(true);
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
        std::cout << inputsString << std::endl;
        std::cout << catalogString << std::endl;
        std::cout << maximumNumberResults << std::endl;
        return "Exception: " + std::string{exc.what()};
    }
}

std::vector<std::string> get_available_core_filters(){
    std::vector<std::string> filters;
    for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreAdviser::CoreAdviserFilters>()) {
        std::string filterString(filter);
        filters.push_back(filterString);
    }
    return filters;
}

std::string calculate_leakage_inductance(std::string magneticString, double frequency, size_t sourceIndex){
    OpenMagnetics::Magnetic magnetic(json::parse(magneticString));

    auto leakageInductanceOutput = OpenMagnetics::LeakageInductance().calculate_leakage_inductance_all_windings(magnetic, frequency, sourceIndex);

    json result;
    to_json(result, leakageInductanceOutput);
    return result.dump(4);
}

std::string calculate_flyback_inputs(std::string flybackInputsString){
    try {
        json flybackInputsJson = json::parse(flybackInputsString);

        OpenMagnetics::Flyback flybackInputs(flybackInputsJson);
        auto inputs = flybackInputs.process();

        json result;
        to_json(result, inputs);
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
        auto inputs = flybackInputs.process();

        json result;
        to_json(result, inputs);
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
        std::cout << std::string{exc.what()} << std::endl;
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
        std::cout << std::string{exc.what()} << std::endl;
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
        std::cout << std::string{exc.what()} << std::endl;
        return {0};
    }
}

std::string create_quick_bobbin(std::string coresString, double thickness){
    OpenMagnetics::Core core(json::parse(coresString), false, true);
    auto bobbin = OpenMagnetics::Bobbin::create_quick_bobbin(core, thickness);

    json result;
    to_json(result, bobbin);
    return result.dump(4);
}


std::string mas_autocomplete(std::string masString, bool simulate, std::string configurationString) {
    try {
        OpenMagnetics::Mas mas(json::parse(masString));
        json configuration(json::parse(configurationString));
        auto autocompletedMas = mas_autocomplete(mas, simulate, configuration);

        json result;
        to_json(result, autocompletedMas);
        return result;
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
        return result;
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


std::string get_settings() {
    try {
        json settingsJson;

        settingsJson["magnetizingInductanceIncludeAirInductance"] = settings->get_magnetizing_inductance_include_air_inductance();
        settingsJson["coilAllowMarginTape"] = settings->get_coil_allow_margin_tape();
        settingsJson["coilAllowInsulatedWire"] = settings->get_coil_allow_insulated_wire();
        settingsJson["coilFillSectionsWithMarginTape"] = settings->get_coil_fill_sections_with_margin_tape();
        settingsJson["coilWindEvenIfNotFit"] = settings->get_coil_wind_even_if_not_fit();
        settingsJson["coilDelimitAndCompact"] = settings->get_coil_delimit_and_compact();
        settingsJson["coilTryRewind"] = settings->get_coil_try_rewind();
        settingsJson["useOnlyCoresInStock"] = settings->get_use_only_cores_in_stock();
        settingsJson["painterNumberPointsX"] = settings->get_painter_number_points_x();
        settingsJson["painterNumberPointsY"] = settings->get_painter_number_points_y();
        settingsJson["painterMirroringDimension"] = settings->get_painter_mirroring_dimension();
        settingsJson["painterMode"] = settings->get_painter_mode();
        settingsJson["painterLogarithmicScale"] = settings->get_painter_logarithmic_scale();
        settingsJson["painterIncludeFringing"] = settings->get_painter_include_fringing();
        if (settings->get_painter_maximum_value_colorbar()) {
            settingsJson["painterMaximumValueColorbar"] = settings->get_painter_maximum_value_colorbar();
        }
        if (settings->get_painter_minimum_value_colorbar()) {
            settingsJson["painterMinimumValueColorbar"] = settings->get_painter_minimum_value_colorbar();
        }
        settingsJson["painterColorFerrite"] = settings->get_painter_color_ferrite();
        settingsJson["painterColorBobbin"] = settings->get_painter_color_bobbin();
        settingsJson["painterColorCopper"] = settings->get_painter_color_copper();
        settingsJson["painterColorInsulation"] = settings->get_painter_color_insulation();
        settingsJson["painterColorMargin"] = settings->get_painter_color_margin();
        settingsJson["magneticFieldNumberPointsX"] = settings->get_magnetic_field_number_points_x();
        settingsJson["magneticFieldNumberPointsY"] = settings->get_magnetic_field_number_points_y();
        settingsJson["magneticFieldMirroringDimension"] = settings->get_magnetic_field_mirroring_dimension();
        settingsJson["magneticFieldIncludeFringing"] = settings->get_magnetic_field_include_fringing();
        settingsJson["coilAdviserMaximumNumberWires"] = settings->get_coil_adviser_maximum_number_wires();
        settingsJson["coreIncludeMargin"] = settings->get_core_adviser_include_margin();
        settingsJson["coreIncludeStacks"] = settings->get_core_adviser_include_stacks();
        settingsJson["coreIncludeDistributedGaps"] = settings->get_core_adviser_include_distributed_gaps();
        settingsJson["verbose"] = settings->get_verbose();

        settingsJson["useToroidalCores"] = settings->get_use_toroidal_cores();
        settingsJson["useConcentricCores"] = settings->get_use_concentric_cores();

        return settingsJson.dump(4);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

void set_settings(std::string settingsString) {
    json settingsJson = json::parse(settingsString);

    settings->set_magnetizing_inductance_include_air_inductance(settingsJson["magnetizingInductanceIncludeAirInductance"]);
    settings->set_coil_allow_margin_tape(settingsJson["coilAllowMarginTape"]);
    settings->set_coil_allow_insulated_wire(settingsJson["coilAllowInsulatedWire"]);
    settings->set_coil_fill_sections_with_margin_tape(settingsJson["coilFillSectionsWithMarginTape"]);
    settings->set_coil_wind_even_if_not_fit(settingsJson["coilWindEvenIfNotFit"]);
    settings->set_coil_delimit_and_compact(settingsJson["coilDelimitAndCompact"]);
    settings->set_coil_try_rewind(settingsJson["coilTryRewind"]);
    settings->set_use_only_cores_in_stock(settingsJson["useOnlyCoresInStock"]);
    settings->set_painter_number_points_x(settingsJson["painterNumberPointsX"]);
    settings->set_painter_number_points_y(settingsJson["painterNumberPointsY"]);
    settings->set_painter_mirroring_dimension(settingsJson["painterMirroringDimension"]);
    settings->set_painter_mode(settingsJson["painterMode"]);
    settings->set_painter_logarithmic_scale(settingsJson["painterLogarithmicScale"]);
    settings->set_painter_include_fringing(settingsJson["painterIncludeFringing"]);
    if (settingsJson.contains("painterMaximumValueColorbar")) {
        settings->set_painter_maximum_value_colorbar(settingsJson["painterMaximumValueColorbar"]);
    }
    if (settingsJson.contains("painterMinimumValueColorbar")) {
        settings->set_painter_minimum_value_colorbar(settingsJson["painterMinimumValueColorbar"]);
    }
    settings->set_painter_color_ferrite(settingsJson["painterColorFerrite"]);
    settings->set_painter_color_bobbin(settingsJson["painterColorBobbin"]);
    settings->set_painter_color_copper(settingsJson["painterColorCopper"]);
    settings->set_painter_color_insulation(settingsJson["painterColorInsulation"]);
    settings->set_painter_color_margin(settingsJson["painterColorMargin"]);
    settings->set_magnetic_field_number_points_x(settingsJson["magneticFieldNumberPointsX"]);
    settings->set_magnetic_field_number_points_y(settingsJson["magneticFieldNumberPointsY"]);
    settings->set_magnetic_field_mirroring_dimension(settingsJson["magneticFieldMirroringDimension"]);
    settings->set_magnetic_field_include_fringing(settingsJson["magneticFieldIncludeFringing"]);
    settings->set_coil_adviser_maximum_number_wires(settingsJson["coilAdviserMaximumNumberWires"]);
    settings->set_core_adviser_include_margin(settingsJson["coreIncludeMargin"]);
    settings->set_core_adviser_include_stacks(settingsJson["coreIncludeStacks"]);
    settings->set_core_adviser_include_distributed_gaps(settingsJson["coreIncludeDistributedGaps"]);
    settings->set_verbose(settingsJson["verbose"]);

    settings->set_use_toroidal_cores(settingsJson["useToroidalCores"]);
    settings->set_use_concentric_cores(settingsJson["useConcentricCores"]);

}
void reset_settings(std::string settingsString) {
    settings->reset();
}

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
    function("calculate_shape_data", &calculate_shape_data);
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
    function("get_available_core_filters", &get_available_core_filters);
    function("load_cores", &load_cores);
    function("clear_loaded_cores", &clear_loaded_cores);
    function("calculate_leakage_inductance", &calculate_leakage_inductance);
    function("calculate_flyback_inputs", &calculate_flyback_inputs);
    function("calculate_advanced_flyback_inputs", &calculate_advanced_flyback_inputs);
    function("get_only_temperature_dependent_indexes", &get_only_temperature_dependent_indexes);
    function("get_only_frequency_dependent_indexes", &get_only_frequency_dependent_indexes);
    function("get_only_magnetic_field_dc_bias_dependent_indexes", &get_only_magnetic_field_dc_bias_dependent_indexes);
    function("create_quick_bobbin", &create_quick_bobbin);
    function("mas_autocomplete", &mas_autocomplete);
    function("calculate_steinmetz_coefficients", &calculate_steinmetz_coefficients);
    function("get_initial_permeability_equations", &get_initial_permeability_equations);
    function("get_settings", &get_settings);
    function("set_settings", &set_settings);
    function("reset_settings", &reset_settings);
    
    register_map<std::string, double>("map<string, double>");
    register_map<std::string, std::string>("map<string, string>");
    // register_map<std::string, std::map<std::string, std::string>>("map<string, map<string, string>>");
    register_vector<std::string>("vector<std::string>");
    register_vector<int>("vector<int>");
    register_vector<double>("vector<double>");
    register_vector<size_t>("vector<size_t>");
}