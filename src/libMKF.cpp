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
#include "Defaults.h"
#include <MAS.hpp>
#include "InputsWrapper.h"
#include "CoreWrapper.h"
#include "Reluctance.h"
#include "MagnetizingInductance.h"
#include "CoreLosses.h"
#include "CoreTemperature.h"
#include "Utils.h"


using namespace OpenMagnetics;
using namespace emscripten;
using json = nlohmann::json;


std::map<std::string, double> get_constants() {
    auto constants = Constants();
    std::map<std::string, double> constantsMap;
    constantsMap["residualGap"] = constants.residualGap;
    constantsMap["minimumNonResidualGap"] = constants.minimumNonResidualGap;
    constantsMap["vacuumPermeability"] = constants.vacuum_permeability;
    return constantsMap;
}

std::string calculate_harmonics(std::string waveformString, double frequency) {
    Waveform waveform;
    from_json(json::parse(waveformString), waveform);

    auto sampledCurrentWaveform = InputsWrapper::get_sampled_waveform(waveform, frequency);
    auto harmonics = InputsWrapper::get_harmonics_data(sampledCurrentWaveform, frequency);

    json result;
    to_json(result, harmonics);
    return result.dump(4);
}

std::string calculate_processed(std::string harmonicsString, std::string waveformString) {
    Waveform waveform;
    Harmonics harmonics;
    from_json(json::parse(waveformString), waveform);
    from_json(json::parse(harmonicsString), harmonics);

    auto processed = InputsWrapper::get_processed_data(harmonics, waveform, true);

    json result;
    to_json(result, processed);
    return result.dump(4);
}

std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    CoreWrapper core(json::parse(coreDataString), includeMaterialData);
    json result;
    to_json(result, core);
    return result.dump(4);
}

std::string get_material_data(std::string materialName){
    auto materialData = find_core_material_by_name(materialName);
    json result;
    to_json(result, materialData);
    return result.dump(4);
}

std::string get_shape_data(std::string shapeName, std::string dataString){
    json data;
    data["coreShapes"] = json::parse(dataString);

    load_databases(data, true);
    std::cout << "databases loaded" << std::endl;

    auto shapeData = find_core_shape_by_name(shapeName);

    json result;
    to_json(result, shapeData);
    return result.dump(4);
}

std::vector<std::string> get_available_shape_families(){
    std::vector<std::string> families;
    for (auto& family : magic_enum::enum_names<CoreShapeFamily>()) {
        std::string familyString(family);
        families.push_back(familyString);
    }
    return families;
}

std::vector<std::string> get_available_core_materials(){
    return get_material_names();
}
std::vector<std::string> get_available_core_shapes(){
    return get_shape_names();
}

std::map<std::string, double> calculate_gap_reluctance(std::string coreGapData, std::string modelNameString){
    std::string modelNameStringUpper = modelNameString;
    std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
    auto modelName = magic_enum::enum_cast<ReluctanceModels>(modelNameStringUpper);

    auto reluctanceModel = ReluctanceModel::factory(modelName.value());
    CoreGap coreGap(json::parse(coreGapData));

    auto coreGapResult = reluctanceModel->get_gap_reluctance(coreGap);
    return coreGapResult;
}

std::string get_gap_reluctance_model_information(){
    json info;
    info["information"] = ReluctanceModel::get_models_information();
    info["errors"] = ReluctanceModel::get_models_errors();
    info["internal_links"] = ReluctanceModel::get_models_internal_links();
    info["external_links"] = ReluctanceModel::get_models_external_links();
    return info.dump(4);
}

double calculate_inductance_from_number_turns_and_gapping(std::string coreData,
                                                          std::string coilData,
                                                          std::string operatingPointData,
                                                          std::string modelsData,
                                                          std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    load_databases(data, true);
    CoreWrapper core(json::parse(coreData));
    CoilWrapper coil(json::parse(coilData));
    OperatingPoint operatingPoint(json::parse(operatingPointData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

    MagnetizingInductance magnetizing_inductance(models);
    double magnetizingInductance = magnetizing_inductance.get_inductance_from_number_turns_and_gapping(core, coil, &operatingPoint);

    return magnetizingInductance;
}


double calculate_number_turns_from_gapping_and_inductance(std::string coreData,
                                                          std::string inputsData,    
                                                          std::string modelsData,
                                                          std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    load_databases(data, true);
    CoreWrapper core(json::parse(coreData));
    InputsWrapper inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

    MagnetizingInductance magnetizing_inductance(models);
    double numberTurns = magnetizing_inductance.get_number_turns_from_gapping_and_inductance(core, &inputs);

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
    load_databases(data, true);

    CoreWrapper core(json::parse(coreData));
    CoilWrapper coil(json::parse(coilData));
    InputsWrapper inputs(json::parse(inputsData));

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();
    GappingType gappingType = magic_enum::enum_cast<GappingType>(gappingTypeString).value();

    MagnetizingInductance magnetizing_inductance(models);
    std::vector<CoreGap> gapping = magnetizing_inductance.get_gapping_from_number_turns_and_inductance(core,
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

    // auto gappingAux = json::array();
    // for (auto &gap: gapping) {
    //     json aux;
    //     to_json(aux, gap);
    //     gappingAux.push_back(aux);
    // }

    // CoreWrapper core(json::parse(coreDataString), includeMaterialData);
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
    load_databases(data, true);

    CoreWrapper core(json::parse(coreData));
    CoilWrapper coil(json::parse(coilData));
    InputsWrapper inputs(json::parse(inputsData));
    auto operatingPoint = inputs.get_operating_point(0);
    OperatingPointExcitation excitation = operatingPoint.get_excitations_per_winding()[0];
    double magnetizingInductance = resolve_dimensional_values(inputs.get_design_requirements().get_magnetizing_inductance());
    if (!excitation.get_current()) {
        auto magnetizingCurrent = InputsWrapper::get_magnetizing_current(excitation, magnetizingInductance);
        excitation.set_current(magnetizingCurrent);
        operatingPoint.get_mutable_excitations_per_winding()[0] = excitation;
    }

    auto defaults = Defaults();

    bool enableTemperatureConvergence = false;

    std::map<std::string, std::string> models = json::parse(modelsData).get<std::map<std::string, std::string>>();

    auto coreLossesModelName = defaults.coreLossesModelDefault;
    if (models.find("coreLosses") != models.end()) {
        std::string modelNameStringUpper = models["coreLosses"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        coreLossesModelName = magic_enum::enum_cast<CoreLossesModels>(modelNameStringUpper).value();
    }
    auto coreTemperatureModelName = defaults.coreTemperatureModelDefault;
    if (models.find("coreTemperature") != models.end()) {
        std::string modelNameStringUpper = models["coreTemperature"];
        std::transform(modelNameStringUpper.begin(), modelNameStringUpper.end(), modelNameStringUpper.begin(), ::toupper);
        coreTemperatureModelName = magic_enum::enum_cast<CoreTemperatureModels>(modelNameStringUpper).value();
    }

    auto coreLossesModel = CoreLossesModel::factory(models);
    auto coreTemperatureModel = CoreTemperatureModel::factory(coreTemperatureModelName);

    MagnetizingInductance magnetizing_inductance(models);

    double temperature = operatingPoint.get_conditions().get_ambient_temperature();
    double temperatureAfterLosses = temperature;
    SignalDescriptor magneticFluxDensity;
    json result;

    do {
        temperature = temperatureAfterLosses;

        excitation = operatingPoint.get_excitations_per_winding()[0];
        operatingPoint.get_mutable_conditions().set_ambient_temperature(temperature);

        magneticFluxDensity = magnetizing_inductance.get_inductance_and_magnetic_flux_density(core, coil, &operatingPoint).second;
        excitation.set_magnetic_flux_density(magneticFluxDensity);

        result = coreLossesModel->get_core_losses(core, excitation, temperature);

        auto temperatureResult = coreTemperatureModel->get_core_temperature(core, result["totalLosses"].get<double>(), temperature);
        temperatureAfterLosses = temperatureResult["maximumTemperature"];
    } while (fabs(temperature - temperatureAfterLosses) / temperatureAfterLosses >= 0.01 && enableTemperatureConvergence);

    result["magneticFluxDensityPeak"] = magneticFluxDensity.get_processed().value().get_peak().value();
    result["magneticFluxDensityAcPeak"] = magneticFluxDensity.get_processed().value().get_peak().value() - magneticFluxDensity.get_processed().value().get_offset();
    result["voltageRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value();
    result["currentRms"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
    result["apparentPower"] = operatingPoint.get_mutable_excitations_per_winding()[0].get_voltage().value().get_processed().value().get_rms().value() * operatingPoint.get_mutable_excitations_per_winding()[0].get_current().value().get_processed().value().get_rms().value();
    result["maximumCoreTemperature"] = temperatureAfterLosses;
    result["maximumCoreTemperatureRise"] = temperatureAfterLosses - operatingPoint.get_conditions().get_ambient_temperature();

    if (models["coreLosses"] == "ROSHEN") {
        result["_hysteresisMajorLoopTop"] = coreLossesModel->_hysteresisMajorLoopTop;
        result["_hysteresisMajorLoopBottom"] = coreLossesModel->_hysteresisMajorLoopBottom;
        result["_hysteresisMajorH"] = coreLossesModel->_hysteresisMajorH;
        result["_hysteresisMinorLoopTop"] = coreLossesModel->_hysteresisMinorLoopTop;
        result["_hysteresisMinorLoopBottom"] = coreLossesModel->_hysteresisMinorLoopBottom;
    }

    return result.dump(4);
}

std::string get_core_losses_model_information(std::string material, std::string dataString){
    json data;
    data["coreMaterials"] = json::parse(dataString);
    load_databases(data, true);
    std::cout << "databases loaded" << std::endl;

    json info;
    info["information"] = CoreLossesModel::get_models_information();
    info["errors"] = CoreLossesModel::get_models_errors();
    info["internal_links"] = CoreLossesModel::get_models_internal_links();
    info["external_links"] = CoreLossesModel::get_models_external_links();
    info["available_models"] = CoreLossesModel::get_methods(material);
    return info.dump(4);
}

std::string get_core_temperature_model_information(){
    json info;
    info["information"] = CoreTemperatureModel::get_models_information();
    info["errors"] = CoreTemperatureModel::get_models_errors();
    info["internal_links"] = CoreTemperatureModel::get_models_internal_links();
    info["external_links"] = CoreTemperatureModel::get_models_external_links();
    return info.dump(4);
}

EMSCRIPTEN_BINDINGS(my_bindings) {
    function("get_constants", &get_constants);
    function("calculate_harmonics", &calculate_harmonics);
    function("calculate_processed", &calculate_processed);
    function("calculate_core_data", &calculate_core_data);
    function("get_material_data", &get_material_data);
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
    
    register_map<std::string, double>("map<string, double>");
    register_map<std::string, std::string>("map<string, string>");
    // register_map<std::string, std::map<std::string, std::string>>("map<string, map<string, string>>");
    register_vector<std::string>("vector<std::string>");
}