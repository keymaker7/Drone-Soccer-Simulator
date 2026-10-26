#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialSoccerLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const FLinearColor Navy(.018f,.032f,.052f,.94f), Edge(.13f,.27f,.34f,.8f);
const FLinearColor White(.89f,.96f,1.f,1.f), Muted(.43f,.59f,.68f,1.f);
const FLinearColor Cyan(.10f,.75f,.96f,1.f), Orange(1.f,.16f,.11f,1.f);
const FLinearColor Green(.25f,.92f,.64f,1.f), Amber(1.f,.71f,.24f,1.f);
FString UnitName(int32 Id) { return FString::Printf(TEXT("%s-%02d"), Id < 5 ? TEXT("IW") : TEXT("WG"), Id % 5 + 1); }
FString TimeLabel(double Seconds) { int32 T=FMath::Max(0,FMath::FloorToInt(Seconds));return FString::Printf(TEXT("%02d:%02d"),T/60,T%60); }
}

AImperialMatchManager* AImperialHUD::FindMatch()
{
    if (!Match.IsValid()) Match = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialMatchManager::StaticClass()));
    return Match.Get();
}
FVector2D AImperialHUD::Screen(float X,float Y) const { return Origin+FVector2D(X,Y)*Scale; }
void AImperialHUD::Rect(float X,float Y,float W,float H,FLinearColor C)
{
    const auto P=Screen(X,Y);DrawRect(C,P.X,P.Y,W*Scale,H*Scale);
}
void AImperialHUD::Text(const FString& V,float X,float Y,float Size,FLinearColor C,bool Right)
{
    UFont* F=GEngine?GEngine->GetMediumFont():nullptr;if(!F||!Canvas)return;
    float W=0,H=0;Canvas->StrLen(F,TEXT("Ag"),W,H);const float S=Size/FMath::Max(1.f,H);
    W=0;if(Right)Canvas->TextSize(F,V,W,H,S,S);
    const auto P=Screen(X-W,Y);DrawText(V,C,P.X,P.Y,F,S*Scale);
}
FString AImperialHUD::Fit(const FString& V,float Size,float Width)const
{
    UFont* F=GEngine?GEngine->GetMediumFont():nullptr;if(!F||!Canvas)return V;
    float W=0,H=0;Canvas->StrLen(F,TEXT("Ag"),W,H);const float S=Size/FMath::Max(1.f,H);
    Canvas->TextSize(F,V,W,H,S,S);if(W<=Width)return V;FString R=V;
    do{R.LeftChopInline(1);Canvas->TextSize(F,R+TEXT("..."),W,H,S,S);}while(W>Width&&R.Len()>0);
    return R+TEXT("...");
}
void AImperialHUD::Panel(float X,float Y,float W,float H)
{
    Rect(X,Y,W,H,Navy);Rect(X,Y,W,1,Edge);Rect(X,Y+H-1,W,1,Edge);
    Rect(X,Y,1,H,Edge);Rect(X+W-1,Y,1,H,Edge);Bounds.Emplace(Screen(X,Y),Screen(X+W,Y+H));
}
void AImperialHUD::Button(FName Name,const FString& Label,float X,float Y,float W,float H,bool Active,bool Warning)
{
    const bool Hover=FBox2D(Screen(X,Y),Screen(X+W,Y+H)).IsInside(Pointer);
    const auto Accent=Warning?Orange:Cyan;
    Rect(X,Y,W,H,Active||Hover?FLinearColor(.045f,.23f,.28f,.97f):FLinearColor(.055f,.10f,.15f,.97f));
    Rect(X,Y,W,1,Active||Hover?Accent:Edge);if(Active||Hover)Rect(X,Y,2,H,Accent);
    Text(Label,X+11,Y+(H-13)*.5f,13,Active||Hover||Warning?Accent:White);
    ButtonTargets.Add({Name,FBox2D(Screen(X,Y),Screen(X+W,Y+H)),ButtonPriority});
}
void AImperialHUD::ClickPointer()
{
    float X=0,Y=0;if(!PlayerOwner||!PlayerOwner->GetMousePosition(X,Y))return;
    const FVector2D Point(X,Y);
    for(int32 I=ButtonTargets.Num()-1;I>=0;--I)
    {
        const auto& Target=ButtonTargets[I];
        if(bRadioPanelOpen&&Target.Priority<100)continue;
        if(Target.Bounds.IsInside(Point)){NotifyHitBoxClick(Target.Name);return;}
    }
}
bool AImperialHUD::IsPointerOverUI(FVector2D P)const {if(bRadioPanelOpen)return true;for(const auto& B:Bounds)if(B.IsInside(P))return true;return false;}

UImperialRadioSubsystem* AImperialHUD::GetRadio() const
{
    UGameInstance* Instance=GetGameInstance();
    return Instance?Instance->GetSubsystem<UImperialRadioSubsystem>():nullptr;
}

void AImperialHUD::ToggleRadioPanel()
{
    bRadioPanelOpen=!bRadioPanelOpen;
    if(bRadioPanelOpen)if(auto* Manager=FindMatch())Manager->ManualSelected();
}

void AImperialHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;Bounds.Reset();ButtonTargets.Reset();ButtonPriority=10;
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialNoHUD")))return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/1000.f);
    Origin=FVector2D((Canvas->SizeX-1600*Scale)*.5f,(Canvas->SizeY-1000*Scale)*.5f);
    Pointer=FVector2D(-1000,-1000);float MX=0,MY=0;
    if(PlayerOwner&&PlayerOwner->GetMousePosition(MX,MY))Pointer=FVector2D(MX,MY);
    auto* M=FindMatch();if(!M)return;
    const auto& Drones=M->GetDrones();auto* Selected=M->GetSelectedDrone();
    if(bCompactView&&!bRadioPanelOpen)
    {
        Panel(470,22,660,58);
        Text(FString::Printf(TEXT("BLUE  %d   :   %d  RED"),M->GetScore(0),M->GetScore(1)),493,38,24,White);
        Text(TimeLabel(M->GetElapsed()),870,41,19,Cyan);
        Text(M->GetView()==1?TEXT("LOS PILOT"):TEXT("OBSERVER"),990,43,13,Muted);
        Panel(24,934,1552,42);
        if(Selected)Text(FString::Printf(TEXT("%s   %s   ALT %.2f m   THR %.0f%%   BAT %.0f%% / %.1f V"),*UnitName(M->GetSelected()),*Selected->GetPilotStatus(),Selected->GetAltitudeMeters(),Selected->GetThrottle()*100,Selected->GetBatteryPercent(),Selected->GetBatteryVoltage()),40,947,13,White);
        Text(TEXT("F10 PANELS  |  C USB"),1548,950,11,Cyan,true);
        return;
    }
    Panel(24,24,1552,102);Panel(24,142,1172,58);Panel(1220,142,356,452);
    Panel(1220,610,356,316);Panel(24,218,260,190);Panel(24,424,260,123);
    Panel(24,736,680,44);Panel(24,798,680,128);Panel(24,944,1552,32);
    Rect(24,24,5,102,Cyan);
    Text(TEXT("IMPERIAL WINGS"),45,42,31,White);
    Text(TEXT("DRONE SOCCER  /  GWANGJU TRAINING CENTRE"),47,88,12,Cyan);
    Rect(500,43,1,64,Edge);
    Text(TEXT("BLUE"),540,43,11,M->GetTeamColor(0));Text(TEXT("RED"),821,43,11,M->GetTeamColor(5));
    Text(FString::Printf(TEXT("SETS  %d"),M->GetSetWins(0)),650,44,10,Muted);
    Text(FString::Printf(TEXT("SETS  %d"),M->GetSetWins(1)),915,44,10,Muted);
    Text(FString::FromInt(M->GetScore(0)),582,61,43,M->GetTeamColor(0));
    Text(TEXT(":"),732,64,35,Muted);Text(FString::FromInt(M->GetScore(1)),864,61,43,M->GetTeamColor(5));
    if(M->IsOffside(0))Text(TEXT("OFFSIDE"),650,85,10,Orange);
    if(M->IsOffside(1))Text(TEXT("OFFSIDE"),915,85,10,Orange);
    Text(Fit(M->GetMatchStatus(),10,415),540,111,10,White);
    Text(TEXT("SESSION / SET CLOCK"),987,43,11,Muted);Text(TimeLabel(M->GetElapsed()),987,66,29,White);
    Text(TEXT("GUIDANCE SOURCE"),1240,43,11,Muted);
    Rect(1242,74,7,7,M->IsConnected()?Green:Amber);
    Text(M->IsConnected()?TEXT("SDACS / APF LINKED"):TEXT("SDACS / OFFLINE"),1259,68,17,M->IsConnected()?Green:Amber);
    const FString Commit=M->GetSourceCommit();
    Text(Commit.IsEmpty()?TEXT("LOCAL PHYSICS READY"):TEXT("SOURCE ")+Commit.Left(10),1241,97,10,Muted);

    float X=36;
    auto Action=[this,&X](FName Name,const FString& Label,float W,bool Active=false,bool Warning=false){Button(Name,Label,X,153,W,36,Active,Warning);X+=W+10;};
    Action(TEXT("Auto"),TEXT("AUTO FLEET"),146,M->IsConnected());
    Action(TEXT("Hold"),TEXT("HOLD"),92);Action(TEXT("Return"),TEXT("RETURN"),112);
    Action(TEXT("Manual"),TEXT("MANUAL"),112);Action(TEXT("Reset"),TEXT("RESET"),88);
    Action(TEXT("Export"),TEXT("EXPORT CSV"),116);
    Action(TEXT("Arm"),Selected&&Selected->IsArmed()?TEXT("DISARM"):TEXT("ARM"),100,Selected&&Selected->IsArmed(),true);
    Action(TEXT("RadioOpen"),TEXT("RADIO / USB"),144,bRadioPanelOpen);
    Button(TEXT("StartMatch"),TEXT("START MATCH"),1028,153,116,36);
    Text(Fit(M->GetModeName(),10,202),987,108,10,Cyan);

    Text(TEXT("TRAINING TELEMETRY"),42,237,13,White);Rect(42,263,224,1,Edge);
    Text(TEXT("Aircraft in arena"),42,282,12,Muted);Text(FString::Printf(TEXT("%02d"),Drones.Num()),265,280,15,White,true);
    Text(TEXT("Closest separation"),42,311,12,Muted);Text(FString::Printf(TEXT("%.2f m"),M->GetClosestSeparation()),265,309,15,Cyan,true);
    Text(TEXT("Close pairs < 0.9 m"),42,340,12,Muted);Text(FString::FromInt(M->GetConflicts()),265,338,15,M->GetConflicts()>0?Amber:White,true);
    Text(TEXT("FIDA 2026 / CLASS 40"),42,377,10,White);
    Text(TEXT("16 x 7 x 4.5 m COURT"),42,394,10,Muted);
    Text(M->GetView()==1?(M->GetSelected()<5?TEXT("LOS PILOT / BLUE"):TEXT("LOS PILOT / RED")):TEXT("TRAINING CAMERAS"),42,441,12,White);
    Button(TEXT("View1"),TEXT("1  LOS"),37,472,70,33,M->GetView()==1);
    Button(TEXT("View2"),TEXT("2  OBS"),115,472,80,33,M->GetView()==2);
    Button(TEXT("View3"),TEXT("3  ARENA"),203,472,69,33,M->GetView()==3);
    Text(TEXT("LOS: RMB HEAD LOOK / WHEEL ZOOM"),42,516,9,Muted);
    Text(TEXT("PHYSICS: EXPERIMENTAL / NOT CALIBRATED"),42,533,8,Amber);

    Text(TEXT("5 v 5  /  TEAM ROSTER"),1238,159,15,White);
    Text(TEXT("AIRCRAFT"),1254,194,10,Muted);Text(TEXT("CONTROL"),1356,194,10,Muted);Text(TEXT("ALT"),1556,194,10,Muted,true);
    for(int32 I=0;I<Drones.Num()&&I<10;++I)
    {
        auto* D=Drones[I].Get();if(!D)continue;const float Y=216+I*35.f;
        const bool Active=I==M->GetSelected();
        if(Active)Rect(1230,Y,336,33,FLinearColor(.035f,.20f,.25f,.97f));
        if(Active)Rect(1230,Y,2,33,M->GetTeamColor(I));
        Rect(1241,Y+13,6,6,M->GetTeamColor(I));
        Text(UnitName(I),1255,Y+7,14,Active?White:Muted);
        Text(D->IsAutonomousEnabled()?TEXT("AUTO"):(D->IsArmed()?TEXT("MANUAL"):TEXT("DISARMED")),1356,Y+8,11,D->IsArmed()?Green:Muted);
        Text(FString::Printf(TEXT("%.1f m"),D->GetAltitudeMeters()),1556,Y+8,12,White,true);
        Rect(1324,Y+10,5,14,D->GetRoleColor());
        ButtonTargets.Add({FName(*FString::Printf(TEXT("Drone_%d"),I)),FBox2D(Screen(1230,Y),Screen(1566,Y+33)),10});
    }
    if(Selected)
    {
        Rect(1220,610,4,44,M->GetTeamColor(M->GetSelected()));
        Text(UnitName(M->GetSelected()),1239,627,23,White);
        Text(ImperialSoccerLayout::RoleName(Selected->GetPlayerRole()),1556,634,10,Selected->GetRoleColor(),true);
        Text(Fit(Selected->GetPilotStatus(),12,313),1240,666,12,Selected->IsArmed()?Green:Amber);
        Rect(1239,694,317,1,Edge);
        Text(TEXT("FLIGHT SPEED"),1240,710,10,Muted);Text(FString::Printf(TEXT("%.2f m/s"),Selected->GetFlightSpeed()),1240,729,24,White);
        Text(TEXT("ALTITUDE"),1411,710,10,Muted);Text(FString::Printf(TEXT("%.2f m"),Selected->GetAltitudeMeters()),1411,729,24,White);
        Text(TEXT("THROTTLE"),1240,775,10,Muted);Text(FString::Printf(TEXT("%.0f %%"),Selected->GetThrottle()*100),1240,794,24,Cyan);
        Text(TEXT("MOTORS"),1411,775,10,Muted);Text(Selected->IsArmed()?TEXT("ARMED"):TEXT("SAFE"),1411,797,18,Selected->IsArmed()?Green:Muted);
        Rect(1240,832,315,3,Edge);Rect(1240,832,315*FMath::Clamp(Selected->GetThrottle(),0.f,1.f),3,M->GetTeamColor(M->GetSelected()));
        Button(TEXT("ArmSelected"),Selected->IsArmed()?TEXT("DISARM SELECTED"):TEXT("ARM SELECTED"),1239,854,316,34,Selected->IsArmed(),true);
        Text(FString::Printf(TEXT("BATTERY %.0f %%  /  %.2f V"),Selected->GetBatteryPercent(),Selected->GetBatteryVoltage()),1240,904,10,Muted);
    }

    Rect(24,736,3,44,Cyan);Text(Fit(M->GetNotice(),12,636),42,752,12,White);
    Text(TEXT("SESSION EVENTS"),42,814,12,White);Rect(42,839,644,1,Edge);
    const auto& Events=M->GetEvents();int32 Row=0;
    for(int32 I=Events.Num()-1;I>=0&&Row<3;--I,++Row)Text(Fit(Events[I],12,640),43,851+Row*23,12,Row==0?White:Muted);
    Text(TEXT("SPACE ARM   ARROWS THROTTLE   WASD PITCH / ROLL   QE YAW"),42,956,10,Cyan);
    Text(TEXT("TAB SELECT   1 LOS / 2 OBSERVER / 3 ARENA"),583,956,10,Muted);
    Text(TEXT("ENTER AUTO   H HOLD   R RETURN   C RADIO"),1109,956,10,Muted);
    Text(FString::Printf(TEXT("%.0f FPS"),M->GetFPS()),1557,956,10,Muted,true);

    // A simple selected-aircraft reticle uses the actual Chaos body location.
    if(Selected&&M->GetView()==3&&PlayerOwner)
    {
        FVector2D P;
        if(PlayerOwner->ProjectWorldLocationToScreen(Selected->GetActorLocation()+FVector(0,0,35),P)&&!IsPointerOverUI(P))
        {
            const auto C=M->GetTeamColor(M->GetSelected());const float R=15*Scale;
            DrawLine(P.X-R,P.Y-R,P.X-R*.4f,P.Y-R,C,1.5f*Scale);DrawLine(P.X-R,P.Y-R,P.X-R,P.Y-R*.4f,C,1.5f*Scale);
            DrawLine(P.X+R,P.Y+R,P.X+R*.4f,P.Y+R,C,1.5f*Scale);DrawLine(P.X+R,P.Y+R,P.X+R,P.Y+R*.4f,C,1.5f*Scale);
            const auto U=(P-Origin)/Scale;Text(UnitName(M->GetSelected()),U.X+22,U.Y-8,12,C);
        }
    }
    if(bRadioPanelOpen)DrawRadioPanel(*M);
}

void AImperialHUD::DrawRadioPanel(AImperialMatchManager& Manager)
{
    DrawRect(FLinearColor(.006f,.012f,.020f,.82f),0,0,Canvas->SizeX,Canvas->SizeY);
    AddHitBox(FVector2D::ZeroVector,FVector2D(Canvas->SizeX,Canvas->SizeY),TEXT("Radio_Backdrop"),true,100);
    Panel(320,126,960,748);Rect(320,126,4,748,Cyan);ButtonPriority=110;
    Text(TEXT("RADIO / USB CONTROLLER"),348,151,25,White);
    Button(TEXT("Radio_Close"),TEXT("CLOSE"),1172,148,80,32);
    UImperialRadioSubsystem* Radio=GetRadio();
    if(!Radio){Text(TEXT("USB controller service is unavailable."),348,212,16,Amber);ButtonPriority=10;return;}
    const FImperialRadioDevice* Device=Radio->GetSelectedDevice();
    const bool Connected=Radio->IsConnected();
    Rect(350,205,7,7,Connected?Green:Amber);
    Text(Connected?TEXT("CONNECTED"):TEXT("DISCONNECTED"),369,199,13,Connected?Green:Amber);
    Text(Fit(Device?Device->Name:TEXT("No USB joystick detected. Connect the radio in USB joystick mode."),16,650),348,224,16,White);
    Text(Device?FString::Printf(TEXT("%d AXES  /  %d BUTTONS  /  DRIVER ID %04X:%04X"),Device->AxisCount,Device->ButtonCount,Device->ManufacturerId,Device->ProductId):TEXT("The device name shown here comes directly from Windows."),348,249,10,Muted);
    Button(TEXT("Radio_Refresh"),TEXT("REFRESH"),1032,205,106,34);
    Button(TEXT("Radio_Device"),TEXT("NEXT DEVICE"),1148,205,104,34);
    Rect(348,276,904,1,Edge);
    Text(TEXT("LIVE STICK AXES"),348,292,12,White);
    Text(TEXT("CHANNEL ASSIGNMENT"),830,292,12,White);
    const TArray<float>& Axes=Radio->GetRawAxes();
    const TCHAR* AxisNames[]={TEXT("X"),TEXT("Y"),TEXT("Z"),TEXT("R"),TEXT("U"),TEXT("V")};
    for(int32 I=0;I<6;++I)
    {
        const float Y=322+I*33.f;
        const bool Available=Connected&&Device&&Device->AxisAvailable.IsValidIndex(I)&&Device->AxisAvailable[I]&&Axes.IsValidIndex(I);
        Text(AxisNames[I],349,Y,13,Available?White:Muted);
        Rect(379,Y+3,340,11,FLinearColor(.05f,.09f,.12f,1));
        Rect(549,Y+1,1,15,Edge);
        if(Available)
        {
            const float Raw=FMath::Clamp(Axes[I],0.f,1.f);
            Rect(379,Y+4,340*Raw,9,Cyan);
            Text(FString::Printf(TEXT("%.3f"),Raw),792,Y,12,White,true);
            if(Radio->GetCaptureSampleCount()>0)
            {
                const float Lo=FMath::Clamp(Radio->GetCapturedMinimum(I),0.f,1.f);
                const float Hi=FMath::Clamp(Radio->GetCapturedMaximum(I),0.f,1.f);
                Rect(379+Lo*340,Y+19,FMath::Max(0.f,Hi-Lo)*340,2,Green);
            }
        }
        else Text(TEXT("--"),792,Y,12,Muted,true);
    }
    const TCHAR* Channels[]={TEXT("ROLL"),TEXT("PITCH"),TEXT("YAW"),TEXT("THROTTLE")};
    const float Values[]={Radio->GetNormalizedRoll(),Radio->GetNormalizedPitch(),Radio->GetNormalizedYaw(),Radio->GetThrottle()};
    TSet<int32> UsedAxes;
    for(int32 I=0;I<4;++I)
    {
        const float Y=321+I*44.f;const int32 Axis=Radio->GetMappedAxis(I);UsedAxes.Add(Axis);
        Text(Channels[I],830,Y+10,11,White);
        Button(FName(*FString::Printf(TEXT("Radio_Axis_%d"),I)),FString::Printf(TEXT("AXIS %s"),Axis>=0&&Axis<6?AxisNames[Axis]:TEXT("?")),919,Y,99,32);
        Button(FName(*FString::Printf(TEXT("Radio_Invert_%d"),I)),Radio->IsInverted(I)?TEXT("INVERTED"):TEXT("NORMAL"),1028,Y,105,32,Radio->IsInverted(I));
        const float V=FMath::Clamp(Values[I],I==3?0.f:-1.f,1.f);
        Text(FString::Printf(TEXT("%+.2f"),V),1251,Y+8,12,Radio->IsCalibrated()?Green:Muted,true);
        Rect(1150,Y+28,101,2,Edge);
        if(Radio->IsCalibrated())Rect(1150,Y+28,101*(I==3?V:(V+1)*.5f),2,Green);
    }
    Text(UsedAxes.Num()==4?TEXT("FOUR DISTINCT AXES ASSIGNED"):TEXT("ASSIGN FOUR DIFFERENT AXES"),830,506,10,UsedAxes.Num()==4?Muted:Amber);
    Text(FString::Printf(TEXT("DEADZONE  %.0f %%"),Radio->GetDeadzone()*100),830,532,11,Muted);
    Button(TEXT("Radio_DeadzoneDown"),TEXT("-"),1104,522,44,28);
    Button(TEXT("Radio_DeadzoneUp"),TEXT("+"),1158,522,44,28);
    Rect(348,562,904,1,Edge);
    Text(TEXT("CALIBRATE THE FOUR STICKS BEFORE ENABLING RADIO CONTROL"),348,580,12,White);
    Text(TEXT("1  START, then move all four sticks through their full ranges. CAPTURE RANGE when finished."),348,605,12,Muted);
    Text(TEXT("2  Center roll / pitch / yaw and move throttle fully down. CAPTURE NEUTRAL, then FINISH & SAVE."),348,627,12,Muted);
    Button(TEXT("Radio_Begin"),TEXT("1  START RANGE"),348,658,180,36,Radio->IsCalibrating());
    Button(TEXT("Radio_Range"),TEXT("2  CAPTURE RANGE"),540,658,180,36);
    Button(TEXT("Radio_Neutral"),TEXT("3  CAPTURE NEUTRAL"),732,658,210,36);
    Button(TEXT("Radio_Finish"),TEXT("4  FINISH & SAVE"),954,658,190,36,Radio->IsCalibrated());
    Rect(348,714,904,46,FLinearColor(.035f,.065f,.09f,1));Rect(348,714,3,46,Radio->IsCalibrated()?Green:Amber);
    Text(Fit(Radio->GetStatus(),13,865),364,729,13,Radio->IsCalibrated()?Green:Amber);
    Button(TEXT("Radio_Keyboard"),TEXT("USE KEYBOARD"),348,783,164,36,!Radio->IsEnabled());
    Button(TEXT("Radio_Enable"),TEXT("ENABLE RADIO"),524,783,183,36,Radio->IsEnabled());
    Button(TEXT("Radio_Save"),TEXT("SAVE PROFILE"),719,783,144,36);
    Button(TEXT("Radio_Load"),TEXT("LOAD PROFILE"),875,783,144,36);
    Text(Radio->IsEnabled()?TEXT("USB RADIO ACTIVE"):TEXT("KEYBOARD ACTIVE"),1252,797,11,Radio->IsEnabled()?Green:Muted,true);
    Text(TEXT("Mapping or inversion changes require calibration again. The app always starts in keyboard mode."),348,844,11,Muted);
    ButtonPriority=10;
}

void AImperialHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);auto* M=FindMatch();if(!M)return;const FString N=BoxName.ToString();
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_UI_CLICK %s"),*N);
    if(N==TEXT("RadioOpen")){ToggleRadioPanel();return;}
    if(bRadioPanelOpen)
    {
        if(N==TEXT("Radio_Close")){bRadioPanelOpen=false;return;}
        auto* R=GetRadio();if(!R)return;
        if(N==TEXT("Radio_Refresh"))R->RefreshDevices();
        else if(N==TEXT("Radio_Device"))R->CycleDevice();
        else if(N.StartsWith(TEXT("Radio_Axis_")))R->CycleAxis(FCString::Atoi(*N.RightChop(11)));
        else if(N.StartsWith(TEXT("Radio_Invert_")))R->ToggleInvert(FCString::Atoi(*N.RightChop(13)));
        else if(N==TEXT("Radio_DeadzoneDown"))R->SetDeadzone(R->GetDeadzone()-.01f);
        else if(N==TEXT("Radio_DeadzoneUp"))R->SetDeadzone(R->GetDeadzone()+.01f);
        else if(N==TEXT("Radio_Begin")){M->ManualSelected();R->BeginCalibration();}
        else if(N==TEXT("Radio_Range"))R->FinishRangeCapture();
        else if(N==TEXT("Radio_Neutral"))R->CaptureNeutral();
        else if(N==TEXT("Radio_Finish"))R->FinishCalibration();
        else if(N==TEXT("Radio_Save"))R->SaveProfile();
        else if(N==TEXT("Radio_Load"))R->LoadProfile();
        else if(N==TEXT("Radio_Enable")){M->ManualSelected();R->EnableRadio();}
        else if(N==TEXT("Radio_Keyboard")){R->DisableRadio();M->ManualSelected();}
        return;
    }
    if(N.StartsWith(TEXT("Drone_")))M->SelectDrone(FCString::Atoi(*N.RightChop(6)));
    else if(N==TEXT("Auto"))M->LaunchSwarm();else if(N==TEXT("Hold"))M->HoldFleet();
    else if(N==TEXT("Return"))M->ReturnFleet();else if(N==TEXT("Manual"))M->ManualSelected();
    else if(N==TEXT("Reset"))M->ResetMatch();else if(N==TEXT("Export"))M->ExportTelemetry();
    else if(N==TEXT("StartMatch"))M->StartMatch();
    else if(N==TEXT("Arm")||N==TEXT("ArmSelected"))M->ToggleArmSelected();else if(N==TEXT("View1"))M->ChangeView(1);
    else if(N==TEXT("View2"))M->ChangeView(2);else if(N==TEXT("View3"))M->ChangeView(3);
}
