#pragma once
#include "../src/LoaderFlow.h"

static void TestLoaderFlow()
{
    using E=LoaderFlow::Event;using P=LoaderFlow::Phase;
    LoaderFlow flow;
    flow.Start(1000);
    require(flow.Observe(2000,0,0,false)==E::None,"waiting for process");
    require(flow.Observe(3000,42,100,false)==E::None,"wait for readiness");
    require(flow.Observe(4000,42,100,true)==E::None,"stabilization starts");
    require(flow.Observe(6999,42,100,true)==E::None,"no premature load");
    require(flow.Observe(7000,42,100,true)==E::Load,"one load when ready");
    require(flow.Observe(8000,42,100,true)==E::None,"no repeated load on timer");
    flow.Finish(true);require(flow.phase()==P::Succeeded,"success stored");
    flow.Start(0);flow.Observe(1,42,100,true);flow.Observe(2000,42,100,false);
    flow.Observe(2500,42,100,true);require(flow.Observe(4000,42,100,true)==E::None,"readiness loss resets stable interval");
    require(flow.Observe(5500,42,100,true)==E::Load,"readiness recovered");
    flow.Start(0);flow.Observe(1,42,100,false);
    require(flow.Observe(2,42,101,true)==E::ProcessExited,"PID reuse rejected");
    flow.Start(0);flow.Observe(1,42,100,false);
    require(flow.Observe(2,0,0,false)==E::ProcessExited,"process exit detected");
    flow.Start(0);flow.Cancel();require(flow.phase()==P::Cancelled,"cancelled");
    require(flow.Observe(10000,42,100,true)==E::None,"no load after cancellation");
    flow.Start(0);require(flow.Observe(LoaderFlow::TimeoutMs,0,0,false)==E::Timeout,"startup timeout");
    require(flow.Observe(LoaderFlow::TimeoutMs+1,42,100,true)==E::None,"no load after timeout");
    flow.Start(0);flow.Observe(1,42,100,true);flow.Observe(3001,42,100,true);flow.Finish(false);
    require(flow.phase()==P::Failed,"engine failure stored");
}
