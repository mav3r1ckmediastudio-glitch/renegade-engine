#pragma once
#include "renegade/bridge/LocationAnnouncement.h"
#include "renegade/bridge/ScreenService.h"
#include <map>
#include <deque>
namespace renegade::bridge
{
    struct WorldLocation
    {
        std::string id,name;
        NavigationPoint position;
        LocationProfile profile;
    };
    struct LocationVisit {WorldLocation location;bool withinRange=false,fired=false,discovered=false;};
    class WorldLocationService
    {
        std::map<std::string,LocationVisit> locations_;
        std::map<std::string,std::string> nativeBaseline_;
        std::deque<std::string> pending_;
        std::optional<LocationAnnouncementFrame> current_;
        bool Queue(const std::string&);
        void StartNext();
    public:
        static constexpr size_t MaximumLocations=512,MaximumQueued=8;
        bool Put(WorldLocation,std::string& error);
        bool SyncNative(WorldLocation,std::string& error);
        bool Remove(const std::string&,std::string& error);
        bool Discover(const std::string&,std::string& error);
        bool Announce(const std::string&,std::string& error);
        void HideAnnouncement();
        void Tick(float dt,std::optional<NavigationPoint> player);
        const LocationVisit* Get(const std::string&)const;
        const std::map<std::string,LocationVisit>& All()const{return locations_;}
        const std::optional<LocationAnnouncementFrame>& Announcement()const{return current_;}
        void Reset();
    };
    ScreenWidget MakeLocationAnnouncementWidget();
}
