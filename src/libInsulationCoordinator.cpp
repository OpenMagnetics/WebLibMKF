#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "constructive_models/Insulation.h"
#include <MAS.hpp>
#include "processors/Inputs.h"


using namespace MAS;
using namespace emscripten;
using json = nlohmann::json;

std::string calculate_insulation(std::string inputsString){
    auto insulationCoordinator = OpenMagnetics::InsulationCoordinator();
    OpenMagnetics::Inputs inputs(json::parse(inputsString), false);

    json result;
    try
    {
        result["creepageDistance"] = insulationCoordinator.calculate_creepage_distance(inputs, true);
        result["clearance"] = insulationCoordinator.calculate_clearance(inputs);
        result["withstandVoltage"] = insulationCoordinator.calculate_withstand_voltage(inputs);
        result["distanceThroughInsulation"] = insulationCoordinator.calculate_distance_through_insulation(inputs);
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
    function("calculate_insulation", &calculate_insulation);
}