#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialRadioSubsystem.h"

void AImperialHUD::ToggleAIPracticePanel()
{
    if(bRadioPanelOpen)ToggleRadioPanel();
    bAIPracticePanel=!bAIPracticePanel;
    bAIResultPage=false;
    if(auto* M=FindMatch();M&&bAIPracticePanel)
    {
        M->ManualSelected();
        const auto S=M->GetAISettings();AIDraftCount=S.Count;AIDraftDifficulty=S.Difficulty;AIDraftSeed=S.Seed;
    }
    if(auto* PC=Cast<AImperialPlayerController>(PlayerOwner))PC->FlushPilotContextKeys();
}
void AImperialHUD::DrawAIPracticePanel(AImperialMatchManager& M)
{
    const FLinearColor White(.89f,.96f,1),Muted(.55f,.69f,.76f),Cyan(.1f,.75f,.96f),Amber(1,.71f,.24f);
    Rect(0,0,1600,1000,FLinearColor(0,0,0,.66f));Panel(320,126,960,748);Rect(320,126,4,748,Cyan);ButtonPriority=110;
    if(bAIResultPage&&M.GetAIResult().Exists()){DrawAIResultPanel(M);ButtonPriority=10;return;}
    Text(TEXT("AI DEFENCE PRACTICE"),348,155,28,White);
    Text(TEXT("Solo goal training  /  Blue striker  /  LOS + ANGLE"),348,197,15,Cyan);
    Text(TEXT("DEFENDERS"),348,249,13,Muted);
    Button(TEXT("AI_Count3"),TEXT("3 DEFENDERS"),348,277,280,42,AIDraftCount==3);
    Button(TEXT("AI_Count4"),TEXT("4 DEFENDERS"),640,277,280,42,AIDraftCount==4);
    if(M.GetAIResult().Exists())Button(TEXT("AI_Last"),TEXT("LAST RESULT"),932,277,320,42);
    Text(TEXT("DIFFICULTY"),348,347,13,Muted);
    Button(TEXT("AI_Easy"),TEXT("BEGINNER"),348,375,280,42,AIDraftDifficulty==0);
    Button(TEXT("AI_Medium"),TEXT("INTERMEDIATE"),640,375,280,42,AIDraftDifficulty==1);
    Button(TEXT("AI_Hard"),TEXT("ADVANCED"),932,375,320,42,AIDraftDifficulty==2);
    Text(TEXT("Slower reaction / patrol"),348,434,12,Muted);
    Text(TEXT("Faster reaction / tracking"),932,434,12,Muted);
    Text(FString::Printf(TEXT("FORMATION SEED   %u"),AIDraftSeed),348,482,18,White);
    Button(TEXT("AI_Seed"),TEXT("NEW RANDOM FORMATION"),850,476,402,38);
    Text(TEXT("Same seed repeats tactical targets. Physics / collisions may differ."),348,531,13,Muted);
    Text(TEXT("Only red defenders launch automatically. You arm the blue striker manually."),348,576,13,White);
    Text(TEXT("3-minute practice limit. STOP AI lands defenders; your throttle stays manual."),348,605,13,White);
    Text(TEXT("Settings lock while running. Official match play remains manual."),348,634,13,Muted);
    Text(M.IsAIPracticeActive()?Fit(M.GetAIStatus(),13,900):TEXT("Ready: all aircraft must be landed and disarmed before START."),348,679,13,Cyan);
    Text(Fit(M.GetNotice(),12,895),348,711,12,Amber);
    if(M.IsAIPracticeActive())Button(TEXT("AI_Stop"),M.IsAILanding()?TEXT("LANDING IN PROGRESS"):TEXT("STOP AI / LAND DEFENDERS"),348,783,550,42,false,true);
    else Button(TEXT("AI_Start"),TEXT("START PRACTICE / RESET PADS & SCORE"),348,783,550,42);
    Button(TEXT("AI_Close"),TEXT("CLOSE"),922,783,330,42);
    Text(TEXT("Local defensive AI. AI team matches and online opponents are separate planned features."),348,846,11,Muted);
    ButtonPriority=10;
}

void AImperialHUD::DrawAIResultPanel(AImperialMatchManager& M)
{
    const auto& R=M.GetAIResult();
    const FLinearColor White(.89f,.96f,1),Muted(.55f,.69f,.76f),Cyan(.1f,.75f,.96f),Amber(1,.71f,.24f);
    Text(TEXT("AI PRACTICE / LAST RESULT"),348,155,27,White);
    Text(FString::Printf(TEXT("%d DEFENDERS  /  %s  /  SEED %u"),R.Settings.Count,R.Settings.Difficulty==0?TEXT("BEGINNER"):(R.Settings.Difficulty==1?TEXT("INTERMEDIATE"):TEXT("ADVANCED")),R.Settings.Seed),348,201,15,Cyan);
    const auto ReasonLabel=[](const FString& Reason)->const TCHAR*
    {
        if(Reason==TEXT("manual_stop"))return TEXT("STOP REQUESTED");if(Reason==TEXT("time_limit"))return TEXT("TIME LIMIT");
        if(Reason==TEXT("landed"))return TEXT("LANDED");if(Reason==TEXT("application_exit"))return TEXT("APP CLOSED");
        if(Reason==TEXT("reset"))return TEXT("RESET");return TEXT("MODE CHANGED");
    };
    Text(FString::Printf(TEXT("STOP: %s   /   FINISH: %s"),ReasonLabel(R.StopReason),ReasonLabel(R.CompletionReason)),348,234,13,R.CompletionReason==TEXT("landed")?White:Amber);
    Panel(348,275,280,95);Panel(640,275,280,95);Panel(932,275,320,95);
    Text(TEXT("VALID GOALS"),367,293,12,Muted);Text(FString::FromInt(R.Goals),367,320,28,Cyan);
    Text(TEXT("PRACTICE TIME"),659,293,12,Muted);Text(FString::Printf(TEXT("%.1f s"),R.PracticeSeconds),659,320,28,White);
    Text(TEXT("AI LANDING TIME"),951,293,12,Muted);Text(FString::Printf(TEXT("%.1f s"),R.LandingSeconds),951,320,28,White);
    Text(TEXT("AIRCRAFT"),348,400,13,Muted);Text(TEXT("GUARD CONTACTS"),680,400,13,Muted);Text(TEXT("GUARD REMAINING"),990,400,13,Muted);
    for(int32 I=0;I<R.Aircraft.Num();++I)
    {
        const auto& A=R.Aircraft[I];const float Y=432+I*38;
        Text(A.Id==0?TEXT("BLUE STRIKER"):FString::Printf(TEXT("RED DEFENDER %d"),A.Id-5),348,Y,15,White);
        Text(FString::FromInt(A.GuardImpacts),680,Y,15,White);Text(FString::Printf(TEXT("%.1f%%"),A.GuardRemaining),990,Y,15,White);
    }
    Text(TEXT("Goals and guard values stop at the stop request. Landing is timed separately."),348,645,12,Muted);
    Text(TEXT("Contacts are per-aircraft registered impacts, not unique collision pairs or scoring attempts."),348,670,12,Muted);
    Text(R.bTestSession?TEXT("AUTOMATED TEST SESSION / no physical pilot evidence"):TEXT("Local practice record / no online ranking verification"),348,700,13,Amber);
    Text(FString::Printf(TEXT("%s   /   %s"),*R.BuildVersion,*R.EndedUtc),348,728,11,Muted);
    Text(R.bSaved?TEXT("SAVED: user data / TrainingResults / AI"):TEXT("NOT SAVED: retry, or discard this unsaved result before starting again."),348,752,12,R.bSaved?Cyan:Amber);
    if(!R.bSaved){Button(TEXT("AI_ResultSave"),TEXT("RETRY SAVE"),348,790,205,38);Button(TEXT("AI_ResultDiscard"),TEXT("DISCARD UNSAVED"),565,790,230,38,false,true);}
    Button(TEXT("AI_ResultBack"),TEXT("SETUP"),807,790,205,38);Button(TEXT("AI_Close"),TEXT("CLOSE"),1024,790,228,38);
    Text(Fit(M.GetNotice(),11,900),348,845,11,Muted);
}
