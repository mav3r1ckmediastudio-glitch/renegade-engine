#include "../EngineBridge/src/RiverWaterfallFlight.h"
#include <iostream>
using namespace renegade::bridge::river_waterfall;
int main() {
    try {
        unsigned cases = 0;
        for (float height : {.5f, 1.1f, 10.f, 20.f, 30.f})
        for (float width : {.5f, 2.f, 4.f, 12.f})
        for (float heading : {0.f, .7f}) {
            Settings settings;
            settings.height = height; settings.width = width;
            settings.heading = heading; settings.origin = {21, 3, -17};
            const auto model = Build(settings);
            Check(model.rows.size() > 500 && model.totalTravel > 0, "Incomplete waterfall");
            float previousTravel = -1, previousAlong = -INFINITY;
            for (const auto& row : model.rows) {
                Check(std::isfinite(row.speed) && row.travel > previousTravel, "Invalid travel order");
                Check(row.centre.y >= settings.origin.y - 1e-5f, "Toe falls below receiver");
                float along = (row.centre.x-settings.origin.x)*model.direction.x + (row.centre.z-settings.origin.z)*model.direction.z;
                Check(along >= previousAlong-1e-5f, "Rounded toe folds upstream");
                Check(std::isfinite(row.tangent.y), "Nonfinite surface tangent");
                previousTravel = row.travel; previousAlong = along;
            }
            Check(std::abs(model.landing.y-3) < 1e-5f, "World landing wrong");
            const float lipDrop = std::min(.35f, height*.15f);
            Check(std::abs(model.lip.y-(3+height-lipDrop)) < 1e-4f, "World lip wrong");
            const float down = std::sqrt(2*9.80665f*lipDrop), time = model.flightSeconds;
            Check(std::abs(down*time+.5f*9.80665f*time*time-(height-lipDrop)) < 1e-4f, "Gravity travel error");
            const auto path = Path(model);
            Check(path.size() == 2048 && path.back().x > 20, "Missing lower reach");
            wi::scene::MeshComponent mesh;
            Mesh(model, mesh, wi::ecs::INVALID_ENTITY);
            Check(mesh.vertex_positions.size() == model.rows.size()*25, "Disconnected sheet rows");
            for (size_t row=0; row<model.rows.size(); ++row) {
                auto a=mesh.vertex_positions[row*25], b=mesh.vertex_positions[row*25+24];
                float distance=std::hypot(a.x-b.x,a.z-b.z);
                Check(std::abs(distance-width)<1e-4f, "Rotated sheet width changed");
            }
            ++cases;
        }
        Settings zero; zero.inletSpeed = 0;
        Check(Build(zero).discharge == 0 && Build(zero).impactPower == 0, "Zero flow emits energy");
        Settings bad; bad.height = 40;
        bool rejected = false;
        try { Build(bad); } catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Unsupported height accepted");
        std::cout << "WATERFALL_FLIGHT_CPU_PASS cases=" << cases << " bounds/rotation/width/gravity/continuous-travel/zero=PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
