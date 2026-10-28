#include "AIRecovery.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace ImperialAI;
int Checks=0;
void Check(bool V){++Checks;if(!V){std::printf("FAIL check %d\n",Checks);std::exit(1);}}
int main()
{
    for(int Rate:{20,30,60,120,240})
    {
        const double Dt=1./Rate;Recovery R;
        for(int I=0;I<Rate*30;++I){const Point P{4.8,.2,2.5};Check(R.Step(Dt,P,P,1,true,true,0)==RecoveryAction::Track);}Check(R.GetAttempts()==0);
        R.Reset();Point P{6.5,1.25,1.2};RecoveryAction A=RecoveryAction::Track;
        for(int I=0;I<Rate*25;++I){A=R.Step(Dt,P,{4.8,0,2.5},1,true,true,0);if(A==RecoveryAction::Failed)break;}
        Check(A==RecoveryAction::Failed&&R.GetAttempts()==2);
        R.Reset();for(int I=0;I<Rate*10;++I){A=R.Step(Dt,P,{4.8,0,2.5},1,true,true,0);if(A==RecoveryAction::Escape)break;}
        Check(A==RecoveryAction::Escape);const Point Escape=R.GetTarget();Check(Escape.X>=1.4&&Escape.X<=6.6&&std::abs(Escape.Y)<=2.8&&Escape.Z>=.65&&Escape.Z<=3.7);
        for(int I=0;I<Rate*3;++I)A=R.Step(Dt,Escape,Escape,1,true,true,0);Check(A==RecoveryAction::Track&&R.GetAttempts()==1);
        // Progressing bodies far from their targets must not be called stuck.
        R.Reset();for(int I=0;I<Rate*30;++I){P={4.+I*Dt*.3,0,2.5};Check(R.Step(Dt,P,{P.X+2,0,2.5},1,true,true,0)==RecoveryAction::Track);}Check(R.GetAttempts()==0);
        R.Reset();for(int I=0;I<Rate;++I)A=R.Step(Dt,{5,0,.2},{5,0,2},-1,true,true,0);Check(A==RecoveryAction::Grounded);
        R.Reset();for(int I=0;I<Rate*2;++I)A=R.Step(Dt,{5,0,2},{5,0,2},1,false,false,0);Check(A==RecoveryAction::Unavailable&&R.GetAttempts()==0);
        R.Reset();for(int I=0;I<Rate*10;++I)A=R.Step(Dt,{5,0,2},{5,0,2},-.9,true,true,0);Check(A==RecoveryAction::Failed);
        R.Reset();Check(R.Step(Dt,{std::numeric_limits<double>::quiet_NaN(),0,2},{},1,true,true,0)==RecoveryAction::Failed);
        R.Reset();Check(R.Step(-1,{},{},1,true,true,0)==RecoveryAction::Track);Check(R.Step(1,{},{},1,true,true,0)==RecoveryAction::Track);
    }
    std::printf("PASS %d recovery supervisor checks\n",Checks);return 0;
}
