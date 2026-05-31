#include <iostream>
#include <vector>
#include "json.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include "constructive_models/Insulation.h"
#include "constructive_models/MasMigration.h"
#include <MAS.hpp>
#include "processors/Inputs.h"


using namespace MAS;
using namespace emscripten;
using json = nlohmann::json;

std::string calculate_insulation(std::string inputsString){
    json result;
    result["creepageDistance"] = 0.0;
    result["clearance"] = 0.0;
    result["withstandVoltage"] = 0.0;
    result["distanceThroughInsulation"] = 0.0;
    result["errorMessage"] = "";
    try
    {
        // Build Inputs via the default ctor + from_json so we bypass
        // check_integrity(); the four insulation calculators read voltage
        // processed peak/rms + altitude + insulation requirements, and do
        // not need a synthesised magnetizing current (which check_integrity
        // tries to build and throws when prerequisites are missing).
        auto j = json::parse(inputsString);
        OpenMagnetics::compat::migrate_pre_1_0(j);
        OpenMagnetics::Inputs inputs;
        from_json(j, inputs);

        auto insulationCoordinator = OpenMagnetics::InsulationCoordinator();
        result["creepageDistance"] = insulationCoordinator.calculate_creepage_distance(inputs, true);
        result["clearance"] = insulationCoordinator.calculate_clearance(inputs);
        result["withstandVoltage"] = insulationCoordinator.calculate_withstand_voltage(inputs);
        result["distanceThroughInsulation"] = insulationCoordinator.calculate_distance_through_insulation(inputs);
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