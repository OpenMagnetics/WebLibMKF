#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "Constants.h"
#include "Insulation.h"
#include "Defaults.h"
#include <MAS.hpp>
#include "NumberTurns.h"
#include "MagneticSimulator.h"
#include "WindingOhmicLosses.h"
#include "WindingSkinEffectLosses.h"
#include "InputsWrapper.h"
#include "CoreWrapper.h"
#include "Reluctance.h"
#include "MagnetizingInductance.h"
#include "CoreLosses.h"
#include "CoreTemperature.h"
#include "Utils.h"


using namespace emscripten;
using json = nlohmann::json;

std::map<std::string, double> get_constants() {
    auto constants = OpenMagnetics::Constants();
    std::map<std::string, double> constantsMap;
    constantsMap["residualGap"] = constants.residualGap;
    constantsMap["minimumNonResidualGap"] = constants.minimumNonResidualGap;
    constantsMap["vacuumPermeability"] = constants.vacuumPermeability;
    return constantsMap;
}

std::string calculate_harmonics(std::string waveformString, double frequency) {
    OpenMagnetics::Waveform waveform;
    OpenMagnetics::from_json(json::parse(waveformString), waveform);

    auto sampledCurrentWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(waveform, frequency);
    auto harmonics = OpenMagnetics::InputsWrapper::calculate_harmonics_data(sampledCurrentWaveform, frequency);

    json result;
    to_json(result, harmonics);
    return result.dump(4);
}

std::string calculate_processed(std::string harmonicsString, std::string waveformString) {
    OpenMagnetics::Waveform waveform;
    OpenMagnetics::Harmonics harmonics;
    OpenMagnetics::from_json(json::parse(waveformString), waveform);
    OpenMagnetics::from_json(json::parse(harmonicsString), harmonics);

    auto processed = OpenMagnetics::InputsWrapper::calculate_processed_data(harmonics, waveform, true);

    json result;
    to_json(result, processed);
    return result.dump(4);
}

std::string calculate_shape_data(std::string shapeString){
    OpenMagnetics::CoreShape shape(json::parse(shapeString));
    OpenMagnetics::CoreWrapper core;
    OpenMagnetics::CoreFunctionalDescription coreFunctionalDescription;
    coreFunctionalDescription.set_shape(shape);
    coreFunctionalDescription.set_material("Dummy");
    coreFunctionalDescription.set_number_stacks(1);
    if (shape.get_magnetic_circuit() == OpenMagnetics::MagneticCircuit::OPEN) {
        coreFunctionalDescription.set_type(OpenMagnetics::CoreType::TWO_PIECE_SET);
    }
    else {
        if (shape.get_family() == OpenMagnetics::CoreShapeFamily::T) {
            coreFunctionalDescription.set_type(OpenMagnetics::CoreType::TOROIDAL);
        }
        else {
            coreFunctionalDescription.set_type(OpenMagnetics::CoreType::CLOSED_SHAPE);
        }
    }
    core.set_functional_description(coreFunctionalDescription);
    core.process_data();

    json result;
    to_json(result, core);
    return result.dump(4);
}

std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    try {
        OpenMagnetics::CoreWrapper core(json::parse(coreDataString), includeMaterialData, true);

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
        OpenMagnetics::MagneticWrapper magnetic(json::parse(magneticString));

        auto optionalBobbin = magnetic.get_coil().get_bobbin();
        OpenMagnetics::BobbinWrapper bobbin;

        if (std::holds_alternative<std::string>(optionalBobbin)) {
            auto bobbinString = std::get<std::string>(optionalBobbin);
            if (bobbinString == "Dummy") {
                bobbin = OpenMagnetics::BobbinWrapper::create_quick_bobbin(magnetic.get_mutable_core());
            }
        }
        else {
            bobbin = OpenMagnetics::BobbinWrapper(std::get<std::string>(optionalBobbin));
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
    auto wire = OpenMagnetics::CoilWrapper::resolve_wire(coilFunctionalDescription);
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
            // Hardcoded
            if (coating->get_grade().value() == 1) {
                json result;
                to_json(result, wire);
                return result.dump(4);
            }
        }
    }

    json result;
    result["errorMessage"] = "Wire not found by standard name";
    return result.dump(4);
}

double get_wire_outer_width_rectangular(double conductingWidth, int grade, std::string wireStandardString){
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_width_rectangular(conductingWidth, grade, wireStandard);
}

double get_wire_outer_height_rectangular(double conductingHeight, int grade, std::string wireStandardString){
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_height_rectangular(conductingHeight, grade, wireStandard);
}

double get_wire_outer_diameter_bare_litz(double conductingDiameter, int numberConductors, int grade, std::string wireStandardString) {
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_diameter_bare_litz(conductingDiameter, numberConductors, grade, wireStandard);
}

double get_wire_outer_diameter_served_litz(double conductingDiameter, int numberConductors, int grade, int numberLayers, std::string wireStandardString) {
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_diameter_served_litz(conductingDiameter, numberConductors, grade, numberLayers, wireStandard);
}

double get_wire_outer_diameter_insulated_litz(double conductingDiameter, int numberConductors, int numberLayers, double thicknessLayers, int grade, std::string wireStandardString) {
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_diameter_insulated_litz(conductingDiameter, numberConductors, numberLayers, thicknessLayers, grade, wireStandard);
}

double get_wire_outer_diameter_enamelled_round(double conductingDiameter, int grade, std::string wireStandardString) {
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_diameter_round(conductingDiameter, grade, wireStandard);
}

double get_wire_outer_diameter_insulated_round(double conductingDiameter, int numberLayers, double thicknessLayers, std::string wireStandardString) {
    OpenMagnetics::WireStandard wireStandard;
    OpenMagnetics::from_json(wireStandardString, wireStandard);
    return OpenMagnetics::WireWrapper::get_outer_diameter_round(conductingDiameter, numberLayers, thicknessLayers, wireStandard);
}

std::vector<double> get_outer_dimensions(std::string wireString) {
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
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
        if (coating->get_type() != OpenMagnetics::InsulationWireCoatingType::ENAMELLED) {
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
        OpenMagnetics::WireWrapper oldWire(json::parse(oldWireString));
        OpenMagnetics::WireType newWireType;
        from_json(json::parse(newWireTypeString), newWireType);

        auto newWire = OpenMagnetics::WireWrapper::get_equivalent_wire(oldWire, newWireType, effectivefrequency);

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
        OpenMagnetics::WireWrapper wire(json::parse(wireString));
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
    OpenMagnetics::InsulationWireCoating insulationWireCoating;
    for (auto wire : wires) {
        auto coatingLabel = wire.encode_coating_label();
        if (coatingLabel == label) {
            if (wire.resolve_coating()) {
                insulationWireCoating = wire.resolve_coating().value();
            }
            else {
                insulationWireCoating.set_type(OpenMagnetics::InsulationWireCoatingType::BARE);
            }
            break;
        }
    }
    json result;
    to_json(result, insulationWireCoating);
    return result.dump(4);
}

std::vector<std::string> get_coating_labels_by_type(std::string wireTypeString){
    OpenMagnetics::WireType wireType(json::parse(wireTypeString));

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
        OpenMagnetics::CoreWrapper core(coreJson, false);
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
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
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

std::vector<std::string> get_available_shape_families(){
    std::vector<std::string> families;
    for (auto& family : magic_enum::enum_names<OpenMagnetics::CoreShapeFamily>()) {
        std::string familyString(family);
        families.push_back(familyString);
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

std::vector<std::string> get_available_core_shape_families(){
    std::vector<std::string> families;
    for (auto& family : magic_enum::enum_names<OpenMagnetics::CoreShapeFamily>()) {
        std::string familyString(family);
        families.push_back(familyString);
    }
    return families;
}

std::vector<std::string> get_available_core_materials(std::string manufacturer){
    return OpenMagnetics::get_material_names(manufacturer);
}

std::vector<std::string> get_available_core_shapes(){
    return OpenMagnetics::get_shape_names();
}

std::vector<std::string> get_available_wires(){
    return OpenMagnetics::get_wire_names();
}

std::vector<std::string> get_unique_wire_diameters(std::string wireStandardString){
    OpenMagnetics::WireStandard wireStandard(json::parse(wireStandardString));

    auto wires = OpenMagnetics::get_wires(OpenMagnetics::WireType::ROUND, wireStandard);

    std::vector<std::string> uniqueStandardName;
    for (auto wire : wires) {
        if (!wire.get_standard_name()) {
            continue;
        }
        auto standardName = wire.get_standard_name().value();
        if (std::find(uniqueStandardName.begin(), uniqueStandardName.end(), standardName) == uniqueStandardName.end()) {
            uniqueStandardName.push_back(standardName);
        }
    }


    return uniqueStandardName;
}

std::vector<std::string> get_available_wire_types(){
    std::vector<std::string> wireTypes;

    for (auto [value, name] : magic_enum::enum_entries<OpenMagnetics::WireType>()) {
        json wireTypeString;
        if (value == OpenMagnetics::WireType::PLANAR) {
            // TODO Add support for planar
            continue;
        }
        OpenMagnetics::to_json(wireTypeString, value);
        wireTypes.push_back(wireTypeString);
    }

    return wireTypes;
}

std::vector<std::string> get_available_wire_standards(){
    std::vector<std::string> wireStandards;

    for (auto [value, name] : magic_enum::enum_entries<OpenMagnetics::WireStandard>()) {
        json wireStandardString;
        OpenMagnetics::to_json(wireStandardString, value);
        wireStandards.push_back(wireStandardString);
    }

    return wireStandards;
}

std::string calculate_gap_reluctance(std::string coreGapData, std::string modelNameString){
    std::string modelNameStringUpper = modelNameString;
    std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
    auto modelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper);

    auto reluctanceModel = OpenMagnetics::ReluctanceModel::factory(modelName.value());
    OpenMagnetics::CoreGap coreGap(json::parse(coreGapData));

    auto coreGapResult = reluctanceModel->get_gap_reluctance(coreGap);
    json result;
    to_json(result, coreGapResult);
    return result.dump(4);
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
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData), false);
    OpenMagnetics::OperatingPoint operatingPoint(json::parse(operatingPointData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
    double magnetizingInductance = magnetizingInductanceObj.calculate_inductance_from_number_turns_and_gapping(core, coil, &operatingPoint).get_magnetizing_inductance().get_nominal().value();

    return magnetizingInductance;
}


double calculate_number_turns_from_gapping_and_inductance(std::string coreData,
                                                          std::string inputsData,    
                                                          std::string modelsData){
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsData));

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
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData), false);
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    OpenMagnetics::GappingType gappingType = magic_enum::enum_cast<OpenMagnetics::GappingType>(gappingTypeString).value();
    
    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizingInductanceObj(reluctanceModelName);
    std::vector<OpenMagnetics::CoreGap> gapping = magnetizingInductanceObj.calculate_gapping_from_number_turns_and_inductance(core,
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
                                  std::string modelsData){

    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData), false);
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsData));
    auto operatingPoint = inputs.get_operating_point(0);
    OpenMagnetics::OperatingPointExcitation excitation = operatingPoint.get_excitations_per_winding()[0];
    double magnetizingInductance = OpenMagnetics::resolve_dimensional_values(inputs.get_design_requirements().get_magnetizing_inductance());
    if (!excitation.get_current()) {
        auto magnetizingCurrent = OpenMagnetics::InputsWrapper::calculate_magnetizing_current(excitation, magnetizingInductance);
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

    OpenMagnetics::MagneticWrapper magnetic;
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

    result["magneticFluxDensityPeak"] = magneticFluxDensity.get_processed().value().get_peak().value();
    result["magneticFluxDensityAcPeak"] = magneticFluxDensity.get_processed().value().get_peak().value() - magneticFluxDensity.get_processed().value().get_offset();
    result["voltageRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value();
    result["currentRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
    result["apparentPower"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value() * operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
    if (coreLossesOutput.get_temperature()) {
        result["maximumCoreTemperature"] = coreLossesOutput.get_temperature().value();
        result["maximumCoreTemperatureRise"] = coreLossesOutput.get_temperature().value() - operatingPoint.get_conditions().get_ambient_temperature();
    }

    return result.dump(4);
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
    OpenMagnetics::OperatingPointExcitation excitation(json::parse(excitationString));

    auto voltage = OpenMagnetics::InputsWrapper::calculate_induced_voltage(excitation, magnetizingInductance);

    json result;
    to_json(result, voltage);
    return result.dump(4);
}

std::string calculate_induced_current(std::string excitationString, double magnetizingInductance){
    OpenMagnetics::OperatingPointExcitation excitation(json::parse(excitationString));

    auto current = OpenMagnetics::InputsWrapper::calculate_magnetizing_current(excitation, magnetizingInductance);

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
    OpenMagnetics::OperatingPointExcitation primaryExcitation(json::parse(primaryExcitationString));

    OpenMagnetics::OperatingPointExcitation excitationOfThisWinding(primaryExcitation);
    auto currentSignalDescriptorProcessed = OpenMagnetics::InputsWrapper::calculate_basic_processed_data(primaryExcitation.get_current().value().get_waveform().value());
    auto voltageSignalDescriptorProcessed = OpenMagnetics::InputsWrapper::calculate_basic_processed_data(primaryExcitation.get_voltage().value().get_waveform().value());

    auto voltageSignalDescriptor = OpenMagnetics::InputsWrapper::reflect_waveform(primaryExcitation.get_voltage().value(), 1.0 / turnRatio, voltageSignalDescriptorProcessed.get_label());
    auto currentSignalDescriptor = OpenMagnetics::InputsWrapper::reflect_waveform(primaryExcitation.get_current().value(), turnRatio, currentSignalDescriptorProcessed.get_label());

    auto voltageSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    voltageSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(voltageSampledWaveform, excitationOfThisWinding.get_frequency()));
    voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));

    auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
    currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));

    excitationOfThisWinding.set_voltage(voltageSignalDescriptor);
    excitationOfThisWinding.set_current(currentSignalDescriptor);

    json result;
    to_json(result, excitationOfThisWinding);
    return result.dump(4);
}

std::string calculate_reflected_primary(std::string secondaryExcitationString, double turnRatio){
    OpenMagnetics::OperatingPointExcitation secondaryExcitation(json::parse(secondaryExcitationString));

    OpenMagnetics::OperatingPointExcitation excitationOfThisWinding(secondaryExcitation);
    auto voltageSignalDescriptor = OpenMagnetics::InputsWrapper::reflect_waveform(secondaryExcitation.get_voltage().value(), turnRatio);
    auto currentSignalDescriptor = OpenMagnetics::InputsWrapper::reflect_waveform(secondaryExcitation.get_current().value(), 1.0 / turnRatio);

    auto voltageSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    voltageSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(voltageSampledWaveform, excitationOfThisWinding.get_frequency()));
    voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));

    auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
    currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));

    excitationOfThisWinding.set_voltage(voltageSignalDescriptor);
    excitationOfThisWinding.set_current(currentSignalDescriptor);

    json result;
    to_json(result, excitationOfThisWinding);
    return result.dump(4);
}

double calculate_instantaneous_power(std::string excitationString){
    OpenMagnetics::OperatingPointExcitation excitation(json::parse(excitationString));

    if (!excitation.get_current().value().get_processed().value().get_rms().value()) {
        auto current = excitation.get_current().value();
        auto processed = OpenMagnetics::InputsWrapper::calculate_processed_data(current.get_harmonics().value(), current.get_waveform().value(), true);
        current.set_processed(processed);
        excitation.set_current(current);
    }
    if (!excitation.get_voltage().value().get_processed().value().get_rms().value()) {
        auto voltage = excitation.get_voltage().value();
        auto processed = OpenMagnetics::InputsWrapper::calculate_processed_data(voltage.get_harmonics().value(), voltage.get_waveform().value(), true);
        voltage.set_processed(processed);
        excitation.set_voltage(voltage);
    }

    auto instantaneousPower = OpenMagnetics::InputsWrapper::calculate_instantaneous_power(excitation);

    return instantaneousPower;
}

double calculate_rms_power(std::string excitationString){
    OpenMagnetics::OperatingPointExcitation excitation(json::parse(excitationString));

    auto voltageSignalDescriptor = excitation.get_voltage().value();
    auto currentSignalDescriptor = excitation.get_current().value();

    if (!voltageSignalDescriptor.get_processed() || !voltageSignalDescriptor.get_processed()->get_rms()) {
        auto voltageSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitation.get_frequency());
        voltageSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(voltageSampledWaveform, excitation.get_frequency()));
        voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true));
    }

    if (!currentSignalDescriptor.get_processed() || !currentSignalDescriptor.get_processed()->get_rms()) {
        auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitation.get_frequency());
        currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitation.get_frequency()));
        currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true));
    }

    double rmsPower = currentSignalDescriptor.get_processed().value().get_rms().value() * voltageSignalDescriptor.get_processed().value().get_rms().value();

    return rmsPower;
}

double resolve_dimension_with_tolerance(std::string dimensionWithToleranceString) {
    OpenMagnetics::DimensionWithTolerance dimensionWithTolerance(json::parse(dimensionWithToleranceString));
    return OpenMagnetics::resolve_dimensional_values(dimensionWithTolerance);
}

std::string calculate_basic_processed_data(std::string waveformString) {
    OpenMagnetics::Waveform waveform(json::parse(waveformString));
    auto processed = OpenMagnetics::InputsWrapper::calculate_basic_processed_data(waveform);
    json result;
    to_json(result, processed);
    return result.dump(4);
}

std::string create_waveform(std::string processedString, double frequency) {
    OpenMagnetics::Processed processed(json::parse(processedString));
    auto waveform = OpenMagnetics::InputsWrapper::create_waveform(processed, frequency);
    json result;
    to_json(result, waveform);
    return result.dump(4);
}

std::string scale_waveform_time_to_frequency(std::string waveformString, double newFrequency) {
    OpenMagnetics::Waveform waveform(json::parse(waveformString));
    auto scaledWaveform = OpenMagnetics::InputsWrapper::scale_time_to_frequency(waveform, newFrequency);
    json result;
    to_json(result, scaledWaveform);
    return result.dump(4);
}

std::string calculate_insulation(std::string inputsString){
    auto standard = OpenMagnetics::InsulationCoordinator();
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString), false);

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
        auto reader = OpenMagnetics::InputsWrapper::CircuitSimulationReader(fileString);
        auto operatingPoint = reader.extract_operating_point(numberWindings, frequency, mapColumnNames);
        operatingPoint = OpenMagnetics::InputsWrapper::process_operating_point(operatingPoint, desiredMagnetizingInductance);
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
    auto reader = OpenMagnetics::InputsWrapper::CircuitSimulationReader(fileString);
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
    auto reader = OpenMagnetics::InputsWrapper::CircuitSimulationReader(fileString);
    auto columnNames = reader.extract_column_names();

    json result = json::array();
    for (auto& columnName : columnNames) {
        result.push_back(columnName);
    }
    return result.dump(4);
}

std::vector<int> calculate_number_turns(int numberTurnsPrimary, std::string designRequirementsString){
    OpenMagnetics::DesignRequirements designRequirements(json::parse(designRequirementsString));

    OpenMagnetics::NumberTurns numberTurns(numberTurnsPrimary, designRequirements);
    auto numberTurnsCombination = numberTurns.get_next_number_turns_combination();

    std::vector<int> numberTurnsResult;
    for (auto turns : numberTurnsCombination) {
        numberTurnsResult.push_back(static_cast<std::make_signed<int>::type>(turns));
    }
    return numberTurnsResult;
}

double calculate_dc_resistance_per_meter(std::string wireString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    auto dcResistancePerMeter = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_meter(wire, temperature);
    return dcResistancePerMeter;
}

double calculate_dc_losses_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    OpenMagnetics::SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    return dcLossesPerMeter;
}

double calculate_skin_ac_factor(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    OpenMagnetics::SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    auto skinAcFactor = (skinLossesPerMeter + dcLossesPerMeter) / dcLossesPerMeter;
    return skinAcFactor;
}

double calculate_skin_ac_losses_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    OpenMagnetics::SignalDescriptor current(json::parse(currentString));
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    return skinLossesPerMeter;
}

double calculate_skin_ac_resistance_per_meter(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    OpenMagnetics::SignalDescriptor current(json::parse(currentString));
    auto dcLossesPerMeter = OpenMagnetics::WindingOhmicLosses::calculate_ohmic_losses_per_meter(wire, current, temperature);
    auto [skinLossesPerMeter, _] = OpenMagnetics::WindingSkinEffectLosses::calculate_skin_effect_losses_per_meter(wire, current, temperature);
    auto skinAcFactor = (skinLossesPerMeter + dcLossesPerMeter) / dcLossesPerMeter;
    auto dcResistancePerMeter = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_meter(wire, temperature);

    return dcResistancePerMeter * skinAcFactor;
}

double calculate_effective_current_density(std::string wireString, std::string currentString, double temperature){
    OpenMagnetics::WireWrapper wire(json::parse(wireString));
    OpenMagnetics::SignalDescriptor current(json::parse(currentString));
    auto effectiveCurrentDensity = wire.calculate_effective_current_density(current, temperature);

    return effectiveCurrentDensity;
}

double calculate_effective_skin_depth(std::string material, std::string currentString, double temperature){
    try {
        OpenMagnetics::SignalDescriptor current(json::parse(currentString));

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
    for (auto& [orientation, _] : magic_enum::enum_entries<OpenMagnetics::WindingOrientation>()) {
        json orientationJson;
        to_json(orientationJson, orientation);
        orientations.push_back(orientationJson);
    }
    return orientations;
}

std::vector<std::string> get_available_coil_alignments(){
    std::vector<std::string> orientations;
    for (auto& [orientation, _] : magic_enum::enum_entries<OpenMagnetics::CoilAlignment>()) {
        json orientationJson;
        to_json(orientationJson, orientation);
        orientations.push_back(orientationJson);
    }
    return orientations;
}

bool check_requirement(std::string requirementString, double value){
    try {
        OpenMagnetics::DimensionWithTolerance requirement(json::parse(requirementString));
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
        OpenMagnetics::CoilWrapper coil;
        coil.set_bobbin(coilJson["bobbin"]);
        coil.set_functional_description(coilFunctionalDescription);
        coil.preload_margins(marginPairs);

        if (coilJson["_layersOrientation"].is_object()) {
            auto layersOrientationPerSection = std::map<std::string, OpenMagnetics::WindingOrientation>(coilJson["_layersOrientation"]);
            for (auto [sectionName, layerOrientation] : layersOrientationPerSection) {
                coil.set_layers_orientation(layerOrientation, sectionName);
            }
        }
        else if (coilJson["_layersOrientation"].is_array()) {
            coil.wind_by_sections(proportionPerWinding, pattern, repetitions);
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto layersOrientationPerSection = std::vector<OpenMagnetics::WindingOrientation>(coilJson["_layersOrientation"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < layersOrientationPerSection.size()) {
                        coil.set_layers_orientation(layersOrientationPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            OpenMagnetics::WindingOrientation layerOrientation(coilJson["_layersOrientation"]);
            coil.set_layers_orientation(layerOrientation);

        }
        if (coilJson["_turnsAlignment"].is_object()) {
            auto turnsAlignmentPerSection = std::map<std::string, OpenMagnetics::CoilAlignment>(coilJson["_turnsAlignment"]);
            for (auto [sectionName, turnsAlignment] : turnsAlignmentPerSection) {
                coil.set_turns_alignment(turnsAlignment, sectionName);
            }
        }
        else if (coilJson["_turnsAlignment"].is_array()) {
            coil.wind_by_sections(proportionPerWinding, pattern, repetitions);
            if (coil.get_sections_description()) {
                auto sections = coil.get_sections_description_conduction();
                auto turnsAlignmentPerSection = std::vector<OpenMagnetics::CoilAlignment>(coilJson["_turnsAlignment"]);
                for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex) {
                    if (sectionIndex < turnsAlignmentPerSection.size()) {
                        coil.set_turns_alignment(turnsAlignmentPerSection[sectionIndex], sections[sectionIndex].get_name());
                    }
                }
            }
        }
        else {
            OpenMagnetics::CoilAlignment turnsAlignment(coilJson["_turnsAlignment"]);
            coil.set_turns_alignment(turnsAlignment);
        }

        bool windResult = false;

        if (proportionPerWinding.size() == coilFunctionalDescription.size()) {
            if (pattern.size() > 0 && repetitions > 0) {
                windResult = coil.wind(proportionPerWinding, pattern, repetitions);
            }
            else if (repetitions > 0) {
                windResult = coil.wind(repetitions);
            }
            else {
                windResult = coil.wind();
            }
        }
        else {
            if (pattern.size() > 0 && repetitions > 0) {
                windResult = coil.wind(pattern, repetitions);
            }
            else if (repetitions > 0) {
                windResult = coil.wind(repetitions);
            }
            else {
                windResult = coil.wind();
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
        OpenMagnetics::CoilWrapper coil;

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
        auto coilSectionsDescription = std::vector<OpenMagnetics::Section>(coilJson["sectionsDescription"]);
        OpenMagnetics::CoilWrapper coil;

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
        auto coilSectionsDescription = std::vector<OpenMagnetics::Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<OpenMagnetics::Layer>(coilJson["layersDescription"]);
        OpenMagnetics::CoilWrapper coil;

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
        auto coilSectionsDescription = std::vector<OpenMagnetics::Section>(coilJson["sectionsDescription"]);
        auto coilLayersDescription = std::vector<OpenMagnetics::Layer>(coilJson["layersDescription"]);
        auto coilTurnsDescription = std::vector<OpenMagnetics::Turn>(coilJson["turnsDescription"]);
        OpenMagnetics::CoilWrapper coil;

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
        OpenMagnetics::CoilWrapper coil(json::parse(coilString), false);

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
        OpenMagnetics::CoilWrapper coil(json::parse(coilString), false);
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
        OpenMagnetics::CoilWrapper coil(json::parse(coilString), false);
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
        OpenMagnetics::CoilWrapper coil(json::parse(coilString), false);
        return coil.are_sections_and_layers_fitting();
    }
    catch (const std::exception &exc) {
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
        return false;
    }
}

std::string add_margin_to_section_by_index(std::string coilString, int sectionIndex, double top_or_left_margin, double bottom_or_right_margin) {
    try {
        OpenMagnetics::CoilWrapper coil(json::parse(coilString), false);
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
        OpenMagnetics::MagneticWrapper magnetic(json::parse(magneticString));
        OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));

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
        OpenMagnetics::BobbinWrapper bobbin(json::parse(bobbinString));
        return bobbin.check_if_fits(dimension, isHorizontalOrRadial);
    }
    catch (const std::exception &exc) {
        std::cout << "Exception: " + std::string{exc.what()} << std::endl;
        return false;
    }
}

EMSCRIPTEN_BINDINGS(my_bindings) {
    function("get_constants", &get_constants);
    function("calculate_harmonics", &calculate_harmonics);
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
    function("get_available_shape_families", &get_available_shape_families);
    function("get_available_core_materials", &get_available_core_materials);
    function("get_available_core_manufacturers", &get_available_core_manufacturers);
    function("get_available_core_shape_families", &get_available_core_shape_families);
    function("get_available_core_shapes", &get_available_core_shapes);
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
    function("calculate_insulation", &calculate_insulation);
    function("extract_operating_point", &extract_operating_point);
    function("extract_map_column_names", &extract_map_column_names);
    function("extract_column_names", &extract_column_names);
    function("calculate_number_turns", &calculate_number_turns);
    function("calculate_dc_resistance_per_meter", &calculate_dc_resistance_per_meter);
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
    
    register_map<std::string, double>("map<string, double>");
    register_map<std::string, std::string>("map<string, string>");
    // register_map<std::string, std::map<std::string, std::string>>("map<string, map<string, string>>");
    register_vector<std::string>("vector<std::string>");
    register_vector<int>("vector<int>");
    register_vector<double>("vector<double>");
}