#include "GuidanceRequestWindow.h"
#include <iostream>
#include <limits>

using namespace ImperialGuidance;
int main()
{
    int Checks=0,Failures=0;
    const auto Check=[&](bool Value,const char* Name){++Checks;if(!Value){++Failures;std::cerr<<"FAIL "<<Name<<'\n';}};
    RequestWindow Window;
    Ticket Mission,Hold,Return;
    Check(Window.Begin(1,Mission),"first request starts");
    Check(!Window.Begin(1.1,Hold),"one request at a time");
    Window.NewCommand();
    Check(Window.Begin(1.2,Hold),"hold request starts after mode change");
    Check(Hold.Command>Mission.Command&&Hold.Request>Mission.Request,"both counters advance");
    Check(!Window.Complete(Mission,1.3),"delayed mission response is rejected");
    Check(Window.IsCurrent(Hold)&&Window.HasPending(),"old response cannot release current hold request");
    Check(Window.Complete(Hold,1.4),"current response accepted");
    Check(!Window.Complete(Hold,1.5),"duplicate callback rejected");
    Window.NewCommand();Window.Begin(2,Return);
    Window.NewCommand(); // Explicit reset while a return is in flight.
    Check(!Window.Complete(Return,2.1),"reset invalidates old return target");
    Window.Begin(3,Mission);
    Check(!Window.Expired(3.5)&&Window.Expired(4.1),"deadline uses elapsed wall time");
    Check(!Window.Complete(Mission,4.1)&&!Window.HasPending(),"expired own response retires without acceptance");
    Window.Begin(5,Hold);Window.Abort();Window.Begin(5.1,Return);
    Check(!Window.Complete(Hold,5.2)&&Window.IsCurrent(Return),"callback after timeout cannot clear next request");
    Check(!Window.Complete(Return,4.9),"backwards wall time rejected");
    const double NaN=std::numeric_limits<double>::quiet_NaN();
    Check(!Window.Begin(NaN,Hold)&&!Window.Begin(-1,Hold),"invalid send time rejected");
    Window.Begin(6,Hold);
    Check(!Window.Complete(Hold,NaN),"invalid completion time rejected");
    bool ReorderingSafe=true;
    for(int I=0;I<1000;++I)
    {
        Window.NewCommand();Window.Begin(10+I,Mission);
        Window.NewCommand();Window.Begin(10.1+I,Hold);
        if(Window.Complete(Mission,10.2+I)||!Window.IsCurrent(Hold)||!Window.Complete(Hold,10.3+I))ReorderingSafe=false;
    }
    Check(ReorderingSafe,"1000 rapid mode changes preserve newest request");
    std::cout<<"Guidance request correlation: "<<Checks<<" checks, "<<Failures<<" failures.\n";
    return Failures?1:0;
}
