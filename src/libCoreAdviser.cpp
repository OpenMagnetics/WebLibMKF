#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <MAS.hpp>
#include "InputsWrapper.h"
#include "MagnetizingInductance.h"
#include "CoreAdviser.h"
#include "Utils.h"
#include "Settings.h"


using namespace emscripten;
using json = nlohmann::json;


std::string calculate_advised_cores(std::string inputsString, std::string weightsString, int maximumNumberResults, bool useOnlyCoresInStock){
    // std::cout << inputsString << std::endl;
    OpenMagnetics::InputsWrapper inputs(json::parse(inputsString));
    std::map<std::string, double> weightsKeysString = json::parse(weightsString);
    std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;

    for (auto const& pair : weightsKeysString) {
        weights[magic_enum::enum_cast<OpenMagnetics::CoreAdviser::CoreAdviserFilters>(pair.first).value()] = pair.second;
    }
    weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::AREA_PRODUCT] = 1;
    weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::ENERGY_STORED] = 1;
    weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::COST] = 1;
    weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::EFFICIENCY] = 1;
    weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::DIMENSIONS] = 1;

    auto settings = OpenMagnetics::Settings::GetInstance();
    settings->set_use_only_cores_in_stock(useOnlyCoresInStock);

    OpenMagnetics::CoreAdviser coreAdviser(false);
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

std::vector<std::string> get_available_core_filters(){
    std::vector<std::string> filters;
    for (auto& filter : magic_enum::enum_names<OpenMagnetics::CoreAdviser::CoreAdviserFilters>()) {
        std::string filterString(filter);
        filters.push_back(filterString);
    }
    return filters;
}


void load_cores(bool includeToroids, bool useOnlyCoresInStock){
    OpenMagnetics::load_cores(includeToroids, useOnlyCoresInStock);
}

void clear_loaded_cores(){
    OpenMagnetics::clear_loaded_cores();
}



EMSCRIPTEN_BINDINGS(my_bindings) {
    function("calculate_advised_cores", &calculate_advised_cores);
    function("get_available_core_filters", &get_available_core_filters);
    function("load_cores", &load_cores);
    function("clear_loaded_cores", &clear_loaded_cores);
    
    register_vector<std::string>("vector<std::string>");
}