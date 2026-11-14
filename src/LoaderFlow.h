#pragma once
#include <cstdint>

// Pure state machine: no Windows calls, launcher, DLL loading or process access.
class LoaderFlow
{
public:
    enum class Phase {Idle, Waiting, Stabilizing, Loading, Succeeded, Failed, Cancelled};
    enum class Event {None, Load, Timeout, ProcessExited};
    static constexpr uint64_t TimeoutMs=180000, StableMs=3000;
private:
    Phase _phase=Phase::Idle;
    uint64_t _start=0,_readySince=0,_identity=0;
    uint32_t _pid=0;
    bool _ready=false;
public:
    Phase phase()const{return _phase;}
    bool waiting()const{return _phase==Phase::Waiting || _phase==Phase::Stabilizing;}
    uint32_t pid()const{return _pid;}
    uint64_t identity()const{return _identity;}
    void Start(uint64_t now){_phase=Phase::Waiting;_start=now;_readySince=0;_identity=0;_pid=0;_ready=false;}
    void Cancel(){if(waiting())_phase=Phase::Cancelled;}
    void Finish(bool okay){_phase=okay?Phase::Succeeded:Phase::Failed;}
    Event Observe(uint64_t now,uint32_t pid,uint64_t identity,bool ready)
    {
        if(!waiting())return Event::None;
        if(now-_start>=TimeoutMs){_phase=Phase::Failed;return Event::Timeout;}
        if(_pid && (!pid || pid!=_pid || identity!=_identity)){_phase=Phase::Failed;return Event::ProcessExited;}
        if(!pid)return Event::None;
        if(!_pid){_pid=pid;_identity=identity;}
        if(!ready){_ready=false;_phase=Phase::Waiting;return Event::None;}
        if(!_ready){_ready=true;_readySince=now;_phase=Phase::Stabilizing;}
        if(now-_readySince>=StableMs){_phase=Phase::Loading;return Event::Load;}
        return Event::None;
    }
};
