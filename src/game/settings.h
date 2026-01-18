#include <ultra64.h>

extern char* gTitleString;

// DISPLAY
extern s8 configFullscreen;
extern unsigned int configDefaultMonitor;
extern s8 configVSync;
extern unsigned int configWindowWidth;
extern unsigned int configWindowHeight;
extern s8 configCustomFullscreenResolution;
extern unsigned int configFullscreenWidth;
extern unsigned int configFullscreenHeight;
extern unsigned int configGraphicsBackend;

// AUDIO
extern float configOverallVolume;
extern float configSeqVolume[];

// GRAPHICS
extern float configInternalResolution;
extern unsigned int configAspectRatio;
extern unsigned int configFrameRate;
extern unsigned int configAntiAliasing;
extern float configDrawDistanceMultiplier;
extern unsigned int configLevelOfDetail;
extern int configTextureFiltering;
extern unsigned int configAnisotropicFiltering;
extern unsigned int configNoiseType;
extern s8 configN64Blur;

// CONTROLS
extern s8 configImprovedControls;
extern s8 configImprovedSwimming;
extern s8 configImprovedHanging;
extern s8 configEnemyBouncing;
extern s8 configDpadControls;
extern s8 configFullAirControl;

// GAMEPLAY
extern unsigned int configApplyBugFixes;
extern s8 configSaveLives;
extern s8 configRespawnCertainItems;
extern s8 configRemoveAnnoyingWarps;
extern s8 configBetterPowerups;
extern s8 configBetterEnemies;
extern s8 configTalkNPCs;
extern s8 configBetterBlastAwayTheWall;
extern s8 configBringMipsBack;
extern s8 configDisableFallDamage;
extern s8 configLeaveAnyTime;

// PROGRESSION
extern s8 configBowsersSub;
extern unsigned int configStayInCourse;
extern s8 configSkipMissionSelect;
extern s8 configSwitchToNextMission;
extern s8 configSkipCutscenes;

// CAMERA
extern unsigned int configDefaultCameraMode;
extern unsigned int configAlternateCameraMode;
extern s8 configImprovedCamera;
extern s8 configVerticalCamera;
extern s8 configImprovedCButtonCamera;
extern s8 configCenterCameraButton;
extern s8 configInvertedCamera;
extern s8 configInvertedVerticalCamera;
extern float configCameraSpeed;
extern float configAdditionalCameraDistance;
extern float configAdditionalFOV;

// HUD AND UI
extern s8 configFixTextTypos;
extern s8 configQuitOption;
extern unsigned int configHudLayout;
extern s8 config4by3Hud;
extern s8 configHudStars;
extern s8 configAddZeroes;
extern s8 configShow100CoinStar;
extern s8 configAlwaysShowHealth;
extern s8 configHUDFiltering;
extern s8 configHideHud;

// EXTRA MOVES
extern s8 configWallSliding;
extern s8 configGroundPoundJump;
extern s8 configSunshineDive;
extern s8 configOdysseyDive;
extern s8 configRolling;
extern s8 configFlashbackGroundPound;

// RESTORATIONS
extern s8 configUnusedPyramidCutscene;
extern s8 configRestoreUnusedSounds;
extern s8 configPenguinSadEyes;
extern s8 configTwirlTripleJump;
extern s8 configBetaLikeCamera;
extern s8 configSpawnSparkles;
extern s8 configReplaceKeysWithStars;

// BONUS MODES
extern s8 configCasualMode;
extern unsigned int configLifeMode;
extern unsigned int configEncoreMode;
extern unsigned int configGreenDemon;
extern s8 configNoHealingMode;
extern s8 configHardSave;
extern s8 configDaredevilSave;
extern s8 configHardcoreSave;
extern s8 configInvisibleMode;

// COLORS
extern unsigned int configColorPalette;
extern unsigned int configColorCap[2][3];
extern unsigned int configColorShirt[2][3];
extern unsigned int configColorOveralls[2][3];
extern unsigned int configColorGloves[2][3];
extern unsigned int configColorShoes[2][3];
extern unsigned int configColorSkin[2][3];
extern unsigned int configColorHair[2][3];
extern s8 configShowCapLogo;

// CHEATS
extern s8 configDebugMovementMode;
extern s8 configDebugCapChanger;
extern s8 configDebugObjectSpawner;
extern unsigned int configMoonJump;
extern unsigned int configBLJEverywhere;
extern s8 configGodMode;
extern s8 configHyperspeedMode;
extern s8 configEasyBowserThrows;
extern s8 configVisibleSecrets;
extern s8 configFlexibleCannons;
extern unsigned int configCoinStarCoins;

// FOR FUN
extern s8 configRockPaperScissors;
extern s8 configAngryPenguin;
extern s8 configPaperMode;
extern s8 configFXMode;
extern s8 configDisableLighting;

// ADVANCED
extern s8 configBlockNonXinputControllers;
extern unsigned int configFullscreenRefreshRate;
extern float configCustomCameraDistance;
extern float configCustomCameraDistanceZoomedOut;

// CONTROLLER MAPPING
extern unsigned int configButtonA;
extern unsigned int configButtonB;
extern unsigned int configButtonStart;
extern unsigned int configButtonL;
extern unsigned int configButtonR;
extern unsigned int configButtonZ;
extern unsigned int configButtonCUp;
extern unsigned int configButtonCDown;
extern unsigned int configButtonCLeft;
extern unsigned int configButtonCRight;
extern unsigned int gControllerLeftDeadzone;
extern unsigned int gControllerRightDeadzone;
extern float configRumbleStrength;

// KEYBOARD MAPPING
extern unsigned int configKeyA;
extern unsigned int configKeyB;
extern unsigned int configKeyStart;
extern unsigned int configKeyL;
extern unsigned int configKeyR;
extern unsigned int configKeyZ;
extern unsigned int configKeyCUp;
extern unsigned int configKeyCDown;
extern unsigned int configKeyCLeft;
extern unsigned int configKeyCRight;
extern unsigned int configKeyStickUp;
extern unsigned int configKeyStickDown;
extern unsigned int configKeyStickLeft;
extern unsigned int configKeyStickRight;
extern unsigned int configKeyWalk;

// MOUSE
extern s8 configMouseCam;
extern float configMouseSensitivity;
extern unsigned int configMouseLeft;
extern unsigned int configMouseRight;
extern unsigned int configMouseMiddle;
extern unsigned int configMouseWheelUp;
extern unsigned int configMouseWheelDown;

extern s16 gCollectedStar;
extern s8 gMarioWillDie;

extern s8 stay_in_level();
extern s8 restart_level_after_star();