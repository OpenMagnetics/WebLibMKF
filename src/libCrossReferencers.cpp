#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <MAS.hpp>
#include "Utils.h"
#include "CoreWrapper.h"
#include "InputsWrapper.h"
#include "CoreCrossReferencer.h"
#include "CoreMaterialCrossReferencer.h"
#include "Settings.h"


using namespace emscripten;
using json = nlohmann::json;
namespace fs = std::filesystem;



std::vector<std::string> get_available_core_materials(std::string manufacturer){
    return OpenMagnetics::get_material_names(manufacturer);
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

std::vector<std::string> get_available_core_shapes(){
    return OpenMagnetics::get_shape_names();
}

std::vector<std::string> get_available_core_shapes_by_manufacturer(std::string manufacturer){
    return OpenMagnetics::get_core_shapes_names(manufacturer);
}

std::string calculate_cross_referenced_core(std::string coreString, int numberTurns, std::string inputsString, int maximumNumberResults, std::string onlyManufacturer, bool useToroidalCores, bool useTwoPieceSetCores, bool useOnlyCoresInStock, bool keepMaterialConstant){
    OpenMagnetics::CoreWrapper referenceCore(json::parse(coreString));
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));

    OpenMagnetics::CoreCrossReferencer coreCrossReferencer;
    if (onlyManufacturer != "") {
        coreCrossReferencer.use_only_manufacturer(onlyManufacturer);
    }
    coreCrossReferencer.use_only_reference_material(keepMaterialConstant);
    if (keepMaterialConstant) {
        coreCrossReferencer.set_limit(100);
    }

    auto settings = OpenMagnetics::Settings::GetInstance();
    if (useToroidalCores != settings->get_use_toroidal_cores() || useTwoPieceSetCores != settings->get_use_concentric_cores() || useOnlyCoresInStock != settings->get_use_only_cores_in_stock()) {
        OpenMagnetics::clear_databases();
        settings->set_use_toroidal_cores(true);
        settings->set_use_concentric_cores(true);
        OpenMagnetics::load_core_shapes();
        settings->set_use_only_cores_in_stock(useOnlyCoresInStock);
        settings->set_use_toroidal_cores(useToroidalCores);
        settings->set_use_concentric_cores(useTwoPieceSetCores);
    }

    auto crossReferencedCores = coreCrossReferencer.get_cross_referenced_core(referenceCore, numberTurns, inputs, maximumNumberResults);
    auto scorings = coreCrossReferencer.get_scorings();
    auto scoredValues = coreCrossReferencer.get_scored_values();
    json results;
    results["cores"] = json::array();
    results["scorings"] = json::array();
    results["data"] = json::array();
    for (auto& [core, scoring] : crossReferencedCores) {
        std::string name = core.get_name().value();

        json coreJson;
        OpenMagnetics::to_json(coreJson, core);
        results["cores"].push_back(coreJson);
        results["scorings"].push_back(scoring);

        json result;
        result["scoringPerFilter"] = json();
        result["scoredValuePerFilter"] = json();
        for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreCrossReferencer::CoreCrossReferencerFilters>()) {
            std::string filterString(filter);
            result["scoringPerFilter"][filterString] = scorings[name][magic_enum::enum_cast<OpenMagnetics::CoreCrossReferencer::CoreCrossReferencerFilters>(filterString).value()];
            result["scoredValuePerFilter"][filterString] = scoredValues[name][magic_enum::enum_cast<OpenMagnetics::CoreCrossReferencer::CoreCrossReferencerFilters>(filterString).value()];
        };
        results["data"].push_back(result);
    }
    results["referenceScoredValues"] = json();


    for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreCrossReferencer::CoreCrossReferencerFilters>()) {
        std::string filterString(filter);
        results["referenceScoredValues"][filterString] = scoredValues["Reference"][magic_enum::enum_cast<OpenMagnetics::CoreCrossReferencer::CoreCrossReferencerFilters>(filterString).value()];
    };

    return results.dump(4);
}

std::string calculate_cross_referenced_core_material(std::string materialName, double temperature, int maximumNumberResults, std::string onlyManufacturer, bool useOnlyCoresInStock){
    auto referenceCoreMaterial = OpenMagnetics::find_core_material_by_name(materialName);

    OpenMagnetics::CoreMaterialCrossReferencer coreMaterialCrossReferencer;
    if (onlyManufacturer != "") {
        coreMaterialCrossReferencer.use_only_manufacturer(onlyManufacturer);
    }
    auto settings = OpenMagnetics::Settings::GetInstance();
    if (useOnlyCoresInStock != settings->get_use_only_cores_in_stock()) {
        OpenMagnetics::clear_databases();
        settings->set_use_only_cores_in_stock(useOnlyCoresInStock);
    }

    auto crossReferencedCoreMaterials = coreMaterialCrossReferencer.get_cross_referenced_core_material(referenceCoreMaterial, temperature, maximumNumberResults);
    std::cout << "coreDatabase.size(): " << coreDatabase.size() << std::endl;
    std::cout << "coreMaterialDatabase.size(): " << coreMaterialDatabase.size() << std::endl;
    std::cout << "referenceCoreMaterial.get_name(): " << referenceCoreMaterial.get_name() << std::endl;
    std::cout << "crossReferencedCoreMaterials.size(): " << crossReferencedCoreMaterials.size() << std::endl;

    auto scorings = coreMaterialCrossReferencer.get_scorings();
    auto scoredValues = coreMaterialCrossReferencer.get_scored_values();
    json results;
    results["coreMaterials"] = json::array();
    results["scorings"] = json::array();
    results["data"] = json::array();
    for (auto& [coreMaterial, scoring] : crossReferencedCoreMaterials) {
        std::string name = coreMaterial.get_name();

        json coreMaterialJson;
        OpenMagnetics::to_json(coreMaterialJson, coreMaterial);
        results["coreMaterials"].push_back(coreMaterialJson);
        results["scorings"].push_back(scoring);

        json result;
        result["scoringPerFilter"] = json();
        result["scoredValuePerFilter"] = json();
        for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreMaterialCrossReferencer::CoreMaterialCrossReferencerFilters>()) {
            std::string filterString(filter);
            result["scoringPerFilter"][filterString] = scorings[name][magic_enum::enum_cast<OpenMagnetics::CoreMaterialCrossReferencer::CoreMaterialCrossReferencerFilters>(filterString).value()];
            result["scoredValuePerFilter"][filterString] = scoredValues[name][magic_enum::enum_cast<OpenMagnetics::CoreMaterialCrossReferencer::CoreMaterialCrossReferencerFilters>(filterString).value()];
        };
        results["data"].push_back(result);
    }
    results["referenceScoredValues"] = json();


    for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreMaterialCrossReferencer::CoreMaterialCrossReferencerFilters>()) {
        std::string filterString(filter);
        results["referenceScoredValues"][filterString] = scoredValues["Reference"][magic_enum::enum_cast<OpenMagnetics::CoreMaterialCrossReferencer::CoreMaterialCrossReferencerFilters>(filterString).value()];
    };

    return results.dump(4);
}

std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    OpenMagnetics::CoreWrapper core(json::parse(coreDataString), includeMaterialData);
    json result;
    to_json(result, core);
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

std::string get_core_material_temperature_dependant_parameters(std::string coreMaterialString, double temperature){
    OpenMagnetics::CoreMaterial coreMaterial(json::parse(coreMaterialString));
    json result;

    result["magneticFluxDensitySaturation"] = OpenMagnetics::CoreWrapper::get_magnetic_flux_density_saturation(coreMaterial, temperature, false);
    result["magneticFieldStrengthSaturation"] = OpenMagnetics::CoreWrapper::get_magnetic_field_strength_saturation(coreMaterial, temperature);
    result["initialPermeability"] = OpenMagnetics::CoreWrapper::get_initial_permeability(coreMaterial, temperature);
    result["resistivity"] = OpenMagnetics::CoreWrapper::get_resistivity(coreMaterial, temperature);
    result["remanence"] = OpenMagnetics::CoreWrapper::get_remanence(coreMaterial, temperature);
    result["coerciveForce"] = OpenMagnetics::CoreWrapper::get_coercive_force(coreMaterial, temperature);

    return result.dump(4);
}


EMSCRIPTEN_BINDINGS(my_bindings) {
    function("get_available_core_materials", &get_available_core_materials);
    function("get_available_core_manufacturers", &get_available_core_manufacturers);
    function("get_available_core_shapes", &get_available_core_shapes);
    function("get_available_core_shapes_by_manufacturer", &get_available_core_shapes_by_manufacturer);
    function("get_available_core_shape_families", &get_available_core_shape_families);
    function("calculate_cross_referenced_core", &calculate_cross_referenced_core);
    function("calculate_cross_referenced_core_material", &calculate_cross_referenced_core_material);
    function("calculate_core_data", &calculate_core_data);
    function("get_core_temperature_dependant_parameters", &get_core_temperature_dependant_parameters);
    function("get_core_material_temperature_dependant_parameters", &get_core_material_temperature_dependant_parameters);
    
    register_vector<std::string>("vector<std::string>");
}