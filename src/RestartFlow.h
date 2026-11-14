#pragma once
#include <cstdint>
class RestartFlow
{
public:
    enum class Event {None,Launch,Timeout};
    enum class Phase {Idle,Closing,Cooling,Finished,Failed,Cancelled};
    static constexpr uint64_t CloseTimeoutMs=30000,CooldownMs=1500;
private:
    Phase _phase=Phase::Idle;uint64_t _start=0,_closed=0;
public:
    bool active()const{return _phase==Phase::Closing || _phase==Phase::Cooling;}
    Phase phase()const{return _phase;}
    void Start(uint64_t now){_start=now;_closed=0;_phase=Phase::Closing;}
    void Cancel(){if(active())_phase=Phase::Cancelled;}
    Event Observe(uint64_t now,bool allStopped)
    {
        if(!active())return Event::None;
        if(!allStopped)
        {
            if(now-_start>=CloseTimeoutMs){_phase=Phase::Failed;return Event::Timeout;}
            return Event::None;
        }
        if(_phase==Phase::Closing){_phase=Phase::Cooling;_closed=now;}
        if(now-_closed>=CooldownMs){_phase=Phase::Finished;return Event::Launch;}
        return Event::None;
    }
};
