#include <iostream>
#include <vector>
#include <vector>
#include <string>
#include <fstream>

#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <MAS.hpp>
#include "InputsWrapper.h"
#include "MagnetizingInductance.h"
#include "WireAdviser.h"
#include "CoilAdviser.h"
#include "CoreAdviser.h"
#include "MagneticAdviser.h"
#include "Utils.h"
#include "Settings.h"
#include "LeakageInductance.h"


using namespace emscripten;
using json = nlohmann::json;
namespace fs = std::filesystem;


std::string calculate_advised_cores(std::string inputsString, std::string weightsString, int maximumNumberResults, bool useOnlyCoresInStock){
    try {

        OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
        std::map<std::string, double> weightsKeysString = json::parse(weightsString);
        std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;

        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::AREA_PRODUCT] = 1;
        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::ENERGY_STORED] = 1;

        double externalSum = 0;
        for (auto const& pair : weightsKeysString) {
            externalSum += pair.second;
        }

        for (auto const& pair : weightsKeysString) {
            weights[magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(pair.first).value()] = pair.second / externalSum;
        }

        auto settings = OpenMagnetics::Settings::GetInstance();
        settings->set_use_only_cores_in_stock(useOnlyCoresInStock);

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
        OpenMagnetics::MasWrapper mas(json::parse(masString));
        json patternJson = json::parse(patternString);
        std::vector<size_t> pattern; 
        for (auto& elem : patternJson) {
            pattern.push_back(elem);
        }

        auto bobbin = mas.get_magnetic().get_coil().get_bobbin();
        if (std::holds_alternative<std::string>(bobbin)) {
            auto bobbinString = std::get<std::string>(bobbin);
            if (bobbinString == "Dummy") {
                mas.get_mutable_magnetic().get_mutable_coil().set_bobbin(OpenMagnetics::BobbinWrapper::create_quick_bobbin(mas.get_mutable_magnetic().get_mutable_core()));
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
        OpenMagnetics::MasWrapper mas(json::parse(masString));

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
        OpenMagnetics::CoilFunctionalDescription coilFunctionalDescription(json::parse(coilFunctionalDescriptionString));
        OpenMagnetics::WireSolidInsulationRequirements wireSolidInsulationRequirements(json::parse(solidInsulationRequirementsString));
        OpenMagnetics::Section section(json::parse(sectionString));
        OpenMagnetics::SignalDescriptor current(json::parse(currentString));

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
        OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
        json patternJson = json::parse(patternString);
        std::vector<size_t> pattern; 
        for (auto& elem : patternJson) {
            pattern.push_back(elem);
        }

        auto results = json::array();
        auto solidInsulationRequirementsCombinations = OpenMagnetics::CoilAdviser().get_solid_insulation_requirements_for_wires(inputs, pattern, repetitions);
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
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
    std::map<std::string, double> weightsKeysString = json::parse(weightsString);
    std::map<OpenMagnetics::MagneticAdviser::MagneticAdviserFilters, double> weights;

    double externalSum = 0;
    for (auto const& pair : weightsKeysString) {
        externalSum += pair.second;
    }

    for (auto const& pair : weightsKeysString) {
        weights[magic_enum::enum_cast<OpenMagnetics::MagneticAdviser::MagneticAdviserFilters>(pair.first).value()] = pair.second / externalSum;
    }

    auto settings = OpenMagnetics::Settings::GetInstance();
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
        result["weightedTotalScoring"] = scorings[name][OpenMagnetics::MagneticAdviser::MagneticAdviserFilters::COST] + scorings[name][OpenMagnetics::MagneticAdviser::MagneticAdviserFilters::EFFICIENCY] + scorings[name][OpenMagnetics::MagneticAdviser::MagneticAdviserFilters::DIMENSIONS];
        result["scoringPerFilter"] = json();
        for (auto& filter : magic_enum::enum_names<OpenMagnetics::MagneticAdviser::MagneticAdviserFilters>()) {
            std::string filterString(filter);
            result["scoringPerFilter"][filterString] = scorings[name][magic_enum::enum_cast<OpenMagnetics::MagneticAdviser::MagneticAdviserFilters>(filterString).value()];
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
        OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
        std::map<OpenMagnetics::MagneticAdviser::MagneticAdviserFilters, double> weights;

        std::vector <OpenMagnetics::MagneticWrapper> catalog;

        for (auto& catalogSubstring : OpenMagnetics::split(catalogString, "\n")) {
            OpenMagnetics::MagneticWrapper magnetic(json::parse(catalogSubstring));
            catalog.push_back(magnetic);
        }

        OpenMagnetics::MagneticAdviser magneticAdviser;
        auto masMagnetics = magneticAdviser.get_advised_magnetic(inputs, catalog, maximumNumberResults);

        auto scorings = magneticAdviser.get_scorings();

        json results = json();
        results["data"] = json::array();
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            std::string name = masMagnetic.get_magnetic().get_manufacturer_info().value().get_reference().value();

            std::cout << "masMagnetic.get_outputs().size()" << std::endl;
            std::cout << masMagnetic.get_outputs().size() << std::endl;
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
        // std::cout << inputsString << std::endl;
        // std::cout << catalogString << std::endl;
        // std::cout << maximumNumberResults << std::endl;
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

size_t load_core_materials(std::string fileToLoad){
    if (fileToLoad != "") {
        OpenMagnetics::load_core_materials(fileToLoad);
    }
    else {
        OpenMagnetics::load_core_materials();
    }

    return coreMaterialDatabase.size();
}

size_t load_core_shapes(std::string fileToLoad){
    if (fileToLoad != "") {
        OpenMagnetics::load_core_shapes(true, fileToLoad);
    }
    else {
        OpenMagnetics::load_core_shapes();
    }
    return coreShapeDatabase.size();
}

size_t load_wires(std::string fileToLoad){
    if (fileToLoad != "") {
        OpenMagnetics::load_wires(fileToLoad);
    }
    else {
        OpenMagnetics::load_wires();
    }
    return wireDatabase.size();
}

void clear_databases(){
    OpenMagnetics::clear_databases();
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

void load_cores(bool includeToroids, bool useOnlyCoresInStock){
    auto settings = OpenMagnetics::Settings::GetInstance();
    settings->set_use_toroidal_cores(includeToroids);
    settings->set_use_only_cores_in_stock(useOnlyCoresInStock);
    OpenMagnetics::load_cores();
}

void clear_loaded_cores(){
    OpenMagnetics::clear_loaded_cores();
}

std::string calculate_leakage_inductance(std::string magneticString, double frequency, size_t sourceIndex){
    OpenMagnetics::MagneticWrapper magnetic(json::parse(magneticString));

    auto leakageInductanceOutput = OpenMagnetics::LeakageInductance().calculate_leakage_inductance_all_windings(magnetic, frequency, sourceIndex);

    json result;
    to_json(result, leakageInductanceOutput);
    return result.dump(4);
}


std::string calculate_core_data(std::string coreDataString, bool includeMaterialData){
    OpenMagnetics::CoreWrapper core(json::parse(coreDataString), includeMaterialData);
    json result;
    to_json(result, core);
    return result.dump(4);
}


std::string get_settings() {
    try {
        auto settings = OpenMagnetics::Settings::GetInstance();
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
    auto settings = OpenMagnetics::Settings::GetInstance();
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
    auto settings = OpenMagnetics::Settings::GetInstance();
    settings->reset();
}


EMSCRIPTEN_BINDINGS(my_bindings) {
    function("calculate_core_data", &calculate_core_data);
    function("calculate_advised_cores", &calculate_advised_cores);
    function("calculate_advised_magnetics", &calculate_advised_magnetics);
    function("calculate_advised_coil", &calculate_advised_coil);
    function("calculate_advised_wires", &calculate_advised_wires);
    function("calculate_advised_sections", &calculate_advised_sections);
    function("calculate_advised_magnetics_from_catalog", &calculate_advised_magnetics_from_catalog);
    function("get_solid_insulation_requirements_for_wires", &get_solid_insulation_requirements_for_wires);
    function("get_available_core_filters", &get_available_core_filters);
    function("load_cores", &load_cores);
    function("clear_loaded_cores", &clear_loaded_cores);
    function("calculate_leakage_inductance", &calculate_leakage_inductance);
    function("get_settings", &get_settings);
    function("set_settings", &set_settings);
    function("reset_settings", &reset_settings);
    function("load_core_materials", &load_core_materials);
    function("load_core_shapes", &load_core_shapes);
    function("load_wires", &load_wires);
    function("clear_databases", &clear_databases);
    function("get_available_core_manufacturers", &get_available_core_manufacturers);
    function("get_available_core_materials", &get_available_core_materials);
    
    register_vector<std::string>("vector<std::string>");
}