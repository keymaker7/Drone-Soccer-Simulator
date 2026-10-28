#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialRadioSubsystem.h"

void AImperialHUD::ToggleAIPracticePanel()
{
    if(bRadioPanelOpen)ToggleRadioPanel();
    bAIPracticePanel=!bAIPracticePanel;
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
    Text(TEXT("AI DEFENCE PRACTICE"),348,155,28,White);
    Text(TEXT("Solo goal training  /  Blue striker  /  LOS + ANGLE"),348,197,15,Cyan);
    Text(TEXT("DEFENDERS"),348,249,13,Muted);
    Button(TEXT("AI_Count3"),TEXT("3 DEFENDERS"),348,277,280,42,AIDraftCount==3);
    Button(TEXT("AI_Count4"),TEXT("4 DEFENDERS"),640,277,280,42,AIDraftCount==4);
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
