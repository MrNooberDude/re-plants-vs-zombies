// @Patoke: implement file
#include "AchievementsScreen.h"
#include "GameSelector.h"
#include "LawnApp.h"
#include "Resources.h"
#include "graphics/Font.h"
#include "lawn/Board.h"
#include "lawn/system/PlayerInfo.h"
#include "widget/WidgetManager.h"

Rect aBackButtonRect = {120, 35, 130, 80};

AchievementItem gAchievementList[MAX_ACHIEVEMENTS] = {
    {"Home Lawn Security",    "Complete Adventure Mode."                                                  },
    {"Nobel Peas Prize",      "Get the golden sunflower trophy."                                          },
    {"Better Off Dead",       "Get to a streak of 10 in I, Zombie Endless"                                },
    {"China Shop",            "Get to a streak of 15 in Vasebreaker Endless"                              },
    {"SPUDOW!",               "Blow up a zombie using a Potato Mine."                                     },
    {"Explodonator",          "Take out 10 full-sized zombies with a single Cherry Bomb."                 },
    {"Morticulturalist",      "Collect all 49 plants (including plants from Crazy Dave's shop)."          },
    {"Don't Pea in the Pool", "Complete a daytime pool level without using pea shooters of any kind."     },
    {"Roll Some Heads",       "Bowl over 5 zombies with a single Wall-Nut."                               },
    {"Grounded",              "Defeat a normal roof level without using any catapult plants."             },
    {"Zombologist",           "Discover the Yeti zombie."                                                 },
    {"Penny Pincher",         "Pick up 30 coins in a row on a single level without letting any disappear."},
    {"Sunny Days",            "Get 8000 sun during a single level."                                       },
    {"Popcorn Party",         "Defeat 2 Gargantuars with Corn Cob missiles in a single level."            },
    {"Good Morning",          "Complete a daytime level by planting only Mushrooms and Coffee Beans."     },
    {"No Fungus Among Us",    "Complete a nighttime level without planting any Mushrooms."                },
    {"Beyond the Grave",      "Beat all 20 mini games."                                                   },
    {"Immortal",              "Survive 20 waves of pure zombie ferocity."                                 },
    {"Towering Wisdom",       "Grow the Tree of Wisdom to 100 feet."                                      },
    {"Mustache Mode",         "Enable Mustache Mode"                                                      }
};

// GOTY @Patoke: 0x401000
AchievementsWidget::AchievementsWidget(LawnApp *theApp) {
    mApp = theApp;
    mWidth = 800;
    mHeight = IMAGE_ACHEESEMENTS_CHINA->mHeight + IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG->mHeight + 15700;
    mScrollDirection = -1;
    mScrollValue = 0;
    mDefaultScrollValue = 30;
    mScrollDecay = 1;
    mDidPressMoreButton = false;
    mMoreRockRect =
        Rect(710, 470, IMAGE_ACHEESEMENTS_MORE_ROCK->mWidth - 25, IMAGE_ACHEESEMENTS_MORE_ROCK->mHeight - 50);
}

// GOTY @Patoke: 0x4010E0
AchievementsWidget::~AchievementsWidget() = default;

// GOTY @Patoke: 0x401A10
void AchievementsWidget::Update() {
    if (mScrollValue <= 0) return;

    if (mScrollValue > mDefaultScrollValue) mScrollValue = mDefaultScrollValue;

    mScrollValue -= mScrollDecay;

    int aNewY = mY + mScrollValue * mScrollDirection;
    if (aNewY >= -1) aNewY = -1;
    // if (aNewY >= mApp->mHeight)
    //	aNewY = mApp->mHeight;

    const int aMaxScroll = 2 * mApp->mHeight + 50 - mHeight;
    if (aNewY <= aMaxScroll) aNewY = aMaxScroll;

    mY = aNewY;

    const int aDelta = aNewY - mY;
    mMoreRockRect.mY += aDelta;
    aBackButtonRect.mY += aDelta;

    if (mScrollValue <= 0) mScrollValue = 0;
}

// GOTY @Patoke: 0x401160
void AchievementsWidget::Draw(Graphics *g) {
    g->DrawImage(IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG, 0, 0);

    const int aHeight = IMAGE_SELECTORSCREEN_ACHIEVEMENTS_BG->mHeight;
    for (int i = 1; i <= 70; i++)
        g->DrawImage(IMAGE_ACHEESEMENTS_HOLE_TILE, 0, aHeight * i);

    g->DrawImage(IMAGE_ACHEESEMENTS_BOOKWORM, 0, 1125);
    g->DrawImage(IMAGE_ACHEESEMENTS_BEJEWELED, 0, 2250);
    g->DrawImage(IMAGE_ACHEESEMENTS_CHUZZLE, 0, 4500);
    g->DrawImage(IMAGE_ACHEESEMENTS_PEGGLE, 0, 6750);
    g->DrawImage(IMAGE_ACHEESEMENTS_PIPE, 0, 9000);
    g->DrawImage(IMAGE_ACHEESEMENTS_ZUMA, 0, 11250);

    g->DrawImage(IMAGE_ACHEESEMENTS_CHINA, 0, mHeight - IMAGE_ACHEESEMENTS_CHINA->mHeight - /*50*/ 650);

    if (aBackButtonRect.Contains(mWidgetManager->mLastMouseX - mX, mWidgetManager->mLastMouseY - mY))
        g->DrawImage(IMAGE_ACHEESEMENTS_BACK_HIGHLIGHT, 128, 55);

    for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
        bool aHasAchievement;
        if (mApp->mPlayerInfo) aHasAchievement = mApp->mPlayerInfo->mEarnedAchievements[i];
        else aHasAchievement = false;

        const int aCurrAchievementOff = 57 * static_cast<int>(i / 2);
        const int aImageXPos = i % 2 == 0 ? 120 : 410;
        const int aImageYPos = 178 + aCurrAchievementOff;
        const int aTextXPos = aImageXPos + 70;
        const int aTextYPos = aImageYPos + 16;

        // Achievement images
        Rect aSrcRect(70 * (i % 7), 70 * (i / 7), 70, 70);
        Rect aDestRect(aImageXPos, aImageYPos, 56, 56);

        g->SetColorizeImages(true);
        g->SetColor(aHasAchievement ? Color(255, 255, 255) : Color(255, 255, 255, 32));

        g->DrawImage(IMAGE_ACHEESEMENTS_ICONS, aDestRect, aSrcRect);
        g->SetColorizeImages(false);

        // Achievement titles
        g->SetFont(FONT_DWARVENTODCRAFT15);
        g->SetColor(Color(21, 175, 0));

        g->DrawString(gAchievementList[i].name, aTextXPos, aTextYPos);

        // Achievement descriptions
        auto aPos = Rect(aTextXPos, aTextYPos + 3, 212, 60);

        g->SetFont(FONT_DWARVENTODCRAFT12);
        g->SetColor(Color(255, 255, 255));

        g->WriteWordWrapped(aPos, gAchievementList[i].description, 12);
    }

    g->DrawImage(IMAGE_ACHEESEMENTS_MORE_ROCK, 700, 450);

    const bool aIsHighlight =
        mMoreRockRect.Contains(mWidgetManager->mLastMouseX - mX, mWidgetManager->mLastMouseY - mY);
    if (mDidPressMoreButton) {
        g->DrawImage(aIsHighlight ? IMAGE_ACHEESEMENTS_TOP_BUTTON_HIGHLIGHT : IMAGE_ACHEESEMENTS_TOP_BUTTON, 700, 450);
    } else {
        g->DrawImage(
            aIsHighlight ? IMAGE_ACHEESEMENTS_MORE_BUTTON_HIGHLIGHT : IMAGE_ACHEESEMENTS_MORE_BUTTON, 700, 450
        );
    }
}

// GOTY @Patoke: 0x4019D0
void AchievementsWidget::KeyDown(const KeyCode theKey) {
    if (theKey == KEYCODE_UP) {
        mScrollValue = mDefaultScrollValue;
        mScrollDirection = 1;
    } else if (theKey == KEYCODE_DOWN) {
        mScrollValue = mDefaultScrollValue;
        mScrollDirection = -1;
    }
}

// GOTY @Patoke: 0x4017F0
void AchievementsWidget::MouseDown(const int x, const int y, const int theClickCount) {
    (void)theClickCount;
    if (aBackButtonRect.Contains(x, y)) mApp->PlaySample(SOUND_GRAVEBUTTON);

    if (mMoreRockRect.Contains(x, y)) mApp->PlaySample(SOUND_GRAVEBUTTON);
}

// GOTY @Patoke: 0x401890
void AchievementsWidget::MouseUp(const int x, const int y, const int theClickCount) {
    (void)theClickCount;
    const auto aPos = Point(x, y);
    if (aBackButtonRect.Contains(aPos)) {
        mApp->mGameSelector->SlideTo(0, 0);
        mApp->mGameSelector->mWidgetManager->SetFocus(mApp->mGameSelector);
    }

    if (mMoreRockRect.Contains(aPos)) {
        mDidPressMoreButton = !mDidPressMoreButton;
        mScrollDirection = mDidPressMoreButton ? -1 : 1;
        mScrollValue = 20;
    }
}

// GOTY @Patoke: 0x4019A0
void AchievementsWidget::MouseWheel(const int theDelta) {
    mScrollValue = mDefaultScrollValue;

    if (theDelta > 0) mScrollDirection = 1;
    else if (theDelta < 0) mScrollDirection = -1;
}

bool ReportAchievement::mAchievementToShow = false;
// GOTY @Patoke: 0x459670
void ReportAchievement::GiveAchievement(LawnApp *theApp, const int theAchievement, const bool theForceGive) {
    // todo @Patoke: finish adding the achievement give events
    if (!theApp->mPlayerInfo) return;

    if (theApp->mPlayerInfo->mEarnedAchievements[theAchievement]) return;

    theApp->mPlayerInfo->mEarnedAchievements[theAchievement] = true;

    if (!theForceGive) return;

    std::string aAchievementName = gAchievementList[theAchievement].name;
    aAchievementName.append(" Achievement!");

    theApp->mBoard->DisplayAdvice(aAchievementName, MESSAGE_STYLE_ACHIEVEMENT, AdviceType::ADVICE_NONE);
    theApp->PlaySample(SOUND_ACHIEVEMENT);

    mAchievementToShow = true;
}

// GOTY @Patoke: 0x44D5B0
void ReportAchievement::AchievementInitForPlayer(LawnApp *theApp) {
    if (!theApp || !theApp->mPlayerInfo) return;

    printf("fixme: ReportAchievement::AchievementInitForPlayer is broken.\n");
    // TODO: Fix this crashing the game
    if (theApp->HasFinishedAdventure()) {
        GiveAchievement(theApp, AchievementId::HomeSecurity, false);
    }

    if (theApp->EarnedGoldTrophy()) {
        GiveAchievement(theApp, AchievementId::NovelPeasPrize, false);
    }

    if (theApp->CanSpawnYetis()) {
        GiveAchievement(theApp, AchievementId::Zombologist, false);
    }

    const int aTreeSize =
        theApp->mPlayerInfo->mChallengeRecords[GAMEMODE_TREE_OF_WISDOM - GAMEMODE_SURVIVAL_NORMAL_STAGE_1];
    if (aTreeSize >= 100) {
        GiveAchievement(theApp, AchievementId::ToweringWisdom, false);
    }

    bool aGiveAchievement = true;
    for (int i = STORE_ITEM_PLANT_GATLINGPEA; i <= STORE_ITEM_PLANT_IMITATER; i++) {
        if (theApp->SeedTypeAvailable(static_cast<SeedType>(i))) aGiveAchievement = false;
    }

    if (aGiveAchievement) {
        GiveAchievement(theApp, AchievementId::Morticulturalist, false);
    }
}
