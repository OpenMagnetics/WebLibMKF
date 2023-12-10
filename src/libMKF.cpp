// #include "libMKF.so"
// #include <cmath>
// #include <filesystem>
// #include <fstream>
// #include <map>
// #include <numbers>
// #include <streambuf>
#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "Constants.h"
#include "Insulation.h"
#include "Defaults.h"
#include <MAS.hpp>
#include "MagneticSimulator.h"
#include "InputsWrapper.h"
#include "CoreWrapper.h"
#include "Reluctance.h"
#include "MagnetizingInductance.h"
#include "CoreLosses.h"
#include "CoreTemperature.h"
#include "CoreAdviser.h"
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

std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    OpenMagnetics::CoreWrapper core(json::parse(coreDataString), includeMaterialData);
    json result;
    to_json(result, core);
    return result.dump(4);
}

std::string load_core_data(std::string coresString, std::string dataString){
    json data;
    data["coreShapes"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);

    json result = json::array();
    for (auto& coreJson : json::parse(coresString)) {
        OpenMagnetics::CoreWrapper core(coreJson, false);
        json aux;
        to_json(aux, core);
        result.push_back(aux);
    }
    return result.dump(4);
}

std::string get_material_data(std::string materialName, std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);

    OpenMagnetics::load_databases(data, true);

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
    result["resistivity"] = core.get_resistivity(temperature);

    return result.dump(4);
}

std::string get_shape_data(std::string shapeName, std::string dataString){
    json data;
    data["coreShapes"] = json::parse(dataString);

    OpenMagnetics::load_databases(data, true);

    auto shapeData = OpenMagnetics::find_core_shape_by_name(shapeName);

    json result;
    to_json(result, shapeData);
    return result.dump(4);
}

std::vector<std::string> get_available_shape_families(){
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
                                                          std::string modelsData,
                                                          std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData));
    OpenMagnetics::OperatingPoint operatingPoint(json::parse(operatingPointData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizing_inductance(reluctanceModelName);
    double magnetizingInductance = magnetizing_inductance.calculate_inductance_from_number_turns_and_gapping(core, coil, &operatingPoint).get_magnetizing_inductance().get_nominal().value();

    return magnetizingInductance;
}


double calculate_number_turns_from_gapping_and_inductance(std::string coreData,
                                                          std::string inputsData,    
                                                          std::string modelsData,
                                                          std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);
    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    
    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizing_inductance(reluctanceModelName);
    double numberTurns = magnetizing_inductance.calculate_number_turns_from_gapping_and_inductance(core, &inputs);

    return numberTurns;
}


std::string calculate_gapping_from_number_turns_and_inductance(std::string coreData,
                                                               std::string coilData,
                                                               std::string inputsData,
                                                               std::string gappingTypeString,
                                                               int decimals,
                                                               std::string modelsData,
                                                               std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);

    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData));
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    OpenMagnetics::GappingType gappingType = magic_enum::enum_cast<OpenMagnetics::GappingType>(gappingTypeString).value();
    
    auto reluctanceModelName = OpenMagnetics::Defaults().reluctanceModelDefault;
    if (models.find("reluctance") != models.end()) {
        std::string modelNameStringUpper = models["reluctance"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        reluctanceModelName = magic_enum::enum_cast<OpenMagnetics::ReluctanceModels>(modelNameStringUpper).value();
    }

    OpenMagnetics::MagnetizingInductance magnetizing_inductance(reluctanceModelName);
    std::vector<OpenMagnetics::CoreGap> gapping = magnetizing_inductance.calculate_gapping_from_number_turns_and_inductance(core,
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
                                  std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);

    OpenMagnetics::CoreWrapper core(json::parse(coreData));
    OpenMagnetics::CoilWrapper coil(json::parse(coilData));
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

    OpenMagnetics::Magnetic magnetic;
    magnetic.set_core(core);
    magnetic.set_coil(coil);

    OpenMagnetics::MagneticSimulator magneticSimulator;
    magneticSimulator.set_core_losses_model_name(coreLossesModelName);
    magneticSimulator.set_core_temperature_model_name(coreTemperatureModelName);
    magneticSimulator.set_reluctance_model_name(reluctanceModelName);
    auto coreLossesOutput = magneticSimulator.calculate_core_loses(operatingPoint, magnetic);
    json result;
    to_json(result, coreLossesOutput);

    OpenMagnetics::MagnetizingInductance magnetizing_inductance(reluctanceModelName);
    auto magneticFluxDensity = magnetizing_inductance.calculate_inductance_and_magnetic_flux_density(core, coil, &operatingPoint).second;

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

std::string get_core_losses_model_information(std::string material, std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);

    json info;
    info["information"] = OpenMagnetics::CoreLossesModel::get_models_information();
    info["errors"] = OpenMagnetics::CoreLossesModel::get_models_errors();
    info["internal_links"] = OpenMagnetics::CoreLossesModel::get_models_internal_links();
    info["external_links"] = OpenMagnetics::CoreLossesModel::get_models_external_links();
    info["available_models"] = OpenMagnetics::CoreLossesModel::get_methods(material);
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
    voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true, true));

    auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
    currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true, true));

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
    voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true, true));

    auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitationOfThisWinding.get_frequency());
    currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitationOfThisWinding.get_frequency()));
    currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true, true));

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

    if (!voltageSignalDescriptor.get_processed()) {
        auto voltageSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(voltageSignalDescriptor.get_waveform().value(), excitation.get_frequency());
        voltageSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(voltageSampledWaveform, excitation.get_frequency()));
        voltageSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(voltageSignalDescriptor, voltageSampledWaveform, true, true));
    }

    if (!currentSignalDescriptor.get_processed()) {
        auto currentSampledWaveform = OpenMagnetics::InputsWrapper::calculate_sampled_waveform(currentSignalDescriptor.get_waveform().value(), excitation.get_frequency());
        currentSignalDescriptor.set_harmonics(OpenMagnetics::InputsWrapper::calculate_harmonics_data(currentSampledWaveform, excitation.get_frequency()));
        currentSignalDescriptor.set_processed(OpenMagnetics::InputsWrapper::calculate_processed_data(currentSignalDescriptor, currentSampledWaveform, true, true));
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

std::string calculate_advised_cores(std::string inputsString, std::string weightsString, std::string coresString, std::string dataString, int maximumNumberResults){
    {
        json data;
        data = json::parse(dataString);
        OpenMagnetics::load_databases(data, true);
    }

    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
    std::vector<OpenMagnetics::CoreWrapper> cores = json::parse(coresString);
    std::map<std::string, double> weightsKeysString = json::parse(weightsString);
    std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;
    for (auto const& pair : weightsKeysString) {
        weights[magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(pair.first).value()] = pair.second;
    }

    OpenMagnetics::CoreAdviser coreAdviser(false);
    auto masMagnetics = coreAdviser.get_advised_core(inputs, weights, &cores, maximumNumberResults);
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
        result["weightedTotalScoring"] = masMagnetic.second;
        result["scoringPerFilter"] = json();
        for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreAdviser::CoreAdviserFilters>()) {
            std::string filterString(filter);
            result["scoringPerFilter"][filterString] = scoring[name][magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(filterString).value()];
        };
        results["data"].push_back(result);
    }
    results["log"] = log;

    return results.dump(4);
}

std::vector<std::string> get_available_core_filters(){
    std::vector<std::string> filters;
    for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreAdviser::CoreAdviserFilters>()) {
        std::string filterString(filter);
        filters.push_back(filterString);
    }
    return filters;
}

std::string calculate_insulation(std::string inputsString, std::string standardsDataString, std::string dataString){
    json data;

    data["wireMaterials"] = json::parse(dataString);
    OpenMagnetics::load_databases(data, true);
    auto standardsData = json::parse(standardsDataString);
    auto standard = OpenMagnetics::InsulationCoordinator(standardsData);
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString), false);

    json result;
    try
    {

        result["creepageDistance"] = standard.calculate_creepage_distance(inputs);
        result["clearance"] = standard.calculate_clearance(inputs);
        result["withstandVoltage"] = standard.calculate_withstand_voltage(inputs);
        result["distanceThroughInsulation"] = standard.calculate_distance_through_insulation(inputs);
        result["errorMessage"] = "";
    }
    catch(const std::runtime_error& re)
    {
        result["errorMessage"] = re.what();
    }
    catch(const std::exception& ex)
    {
        result["errorMessage"] = ex.what();
    }
    catch(...)
    {
        result["errorMessage"] = "Unknown failure occurred. Possible memory corruption";
    }
    return result.dump(4);
}



EMSCRIPTEN_BINDINGS(my_bindings) {
    function("get_constants", &get_constants);
    function("calculate_harmonics", &calculate_harmonics);
    function("calculate_processed", &calculate_processed);
    function("calculate_core_data", &calculate_core_data);
    function("load_core_data", &load_core_data);
    function("get_material_data", &get_material_data);
    function("get_core_temperature_dependant_parameters", &get_core_temperature_dependant_parameters);
    function("get_shape_data", &get_shape_data);
    function("get_available_shape_families", &get_available_shape_families);
    function("get_available_core_materials", &get_available_core_materials);
    function("get_available_core_shapes", &get_available_core_shapes);
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
    function("calculate_advised_cores", &calculate_advised_cores);
    function("get_available_core_filters", &get_available_core_filters);
    function("calculate_insulation", &calculate_insulation);
    
    register_map<std::string, double>("map<string, double>");
    register_map<std::string, std::string>("map<string, string>");
    // register_map<std::string, std::map<std::string, std::string>>("map<string, map<string, string>>");
    register_vector<std::string>("vector<std::string>");
}