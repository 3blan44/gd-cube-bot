#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <vector>
#include <algorithm>

using namespace geode::prelude;

// =====================================================================
//  ВНИМАНИЕ: физические константы ниже — ПРИБЛИЗИТЕЛЬНЫЕ.
//  Настоящие формулы физики GD не опубликованы официально, здесь
//  используются грубые оценки сообщества. Их придётся подбирать
//  вручную под конкретную версию игры и скорость уровня (portal speed).
// =====================================================================
namespace BotPhysics {
    constexpr float GRAVITY        = 0.958f; // ускорение падения за шаг симуляции
    constexpr float JUMP_VELOCITY  = 11.18f; // импульс прыжка
    constexpr float MAX_FALL_SPEED = 15.f;
    constexpr float FORWARD_SPEED  = 5.2f;   // условная скорость по X за шаг (тюнить под speed-порталы)
    constexpr int   LOOKAHEAD_STEPS = 40;    // сколько шагов вперёд симулируем
    constexpr float LOOKAHEAD_RANGE = 250.f; // на сколько юнитов вперёд смотрим на объекты
    constexpr float PLAYER_HALF_SIZE = 15.f; // половина габарита куба (примерно)
}

struct SimState {
    float y;
    float vy;
    bool onGround;
};

static SimState simulateStep(SimState s, bool pressingJump, float groundY) {
    if (s.onGround && pressingJump) {
        s.vy = BotPhysics::JUMP_VELOCITY;
        s.onGround = false;
    }
    s.vy -= BotPhysics::GRAVITY;
    if (s.vy < -BotPhysics::MAX_FALL_SPEED) s.vy = -BotPhysics::MAX_FALL_SPEED;
    s.y += s.vy;
    if (s.y <= groundY) {
        s.y = groundY;
        s.vy = 0.f;
        s.onGround = true;
    }
    return s;
}

// Список ID шипов/опасных объектов. Проверяйте и дополняйте под свою
// версию игры — это лишь распространённые классические ID.
static bool isHazardID(int id) {
    static const std::vector<int> hazardIDs = {8, 39, 61, 243, 392, 393, 394, 397, 398, 399};
    return std::find(hazardIDs.begin(), hazardIDs.end(), id) != hazardIDs.end();
}

class $modify(BotPlayLayer, PlayLayer) {
    struct Fields {
        bool botEnabled = true;
    };

    void update(float dt) {
        PlayLayer::update(dt);

        if (!m_fields->botEnabled) return;
        if (!m_player1 || m_player1->m_isDead) return;

        // Бот работает только в куб-режиме (без корабля/шара/волны/робота/паука)
        if (m_player1->m_isShip || m_player1->m_isBall || m_player1->m_isBird ||
            m_player1->m_isDart || m_player1->m_isRobot || m_player1->m_isSpider) {
            return;
        }

        auto playerPos = m_player1->getPosition();
        float groundY = playerPos.y - (m_player1->m_isOnGround ? 0.f : 0.f); // земля считается текущим уровнем y

        std::vector<CCRect> hazards;
        if (m_objects) {
            for (unsigned int i = 0; i < m_objects->count(); i++) {
                auto obj = static_cast<GameObject*>(m_objects->objectAtIndex(i));
                if (!obj) continue;

                float dx = obj->getPositionX() - playerPos.x;
                if (dx < -10.f || dx > BotPhysics::LOOKAHEAD_RANGE) continue;

                if (isHazardID(obj->m_objectID)) {
                    hazards.push_back(obj->getObjectRect());
                }
            }
        }

        bool shouldJump = decideJump(playerPos, hazards, groundY);

        if (shouldJump) {
            m_player1->pushButton(PlayerButton::Jump);
        } else {
            m_player1->releaseButton(PlayerButton::Jump);
        }
    }

    // Симулируем два варианта будущего: "не прыгать" и "прыгнуть сейчас".
    // Прыгаем, только если это единственный вариант, который не задевает препятствие.
    bool decideJump(CCPoint playerPos, const std::vector<CCRect>& hazards, float groundY) {
        if (hazards.empty()) return false;

        if (!willCollide(playerPos, hazards, groundY, false)) {
            return false; // прыгать не нужно, всё чисто
        }
        // "Ничего не делать" ведёт к столкновению — проверяем, спасает ли прыжок
        return !willCollide(playerPos, hazards, groundY, true);
    }

    bool willCollide(CCPoint playerPos, const std::vector<CCRect>& hazards, float groundY, bool jumpNow) {
        SimState s { playerPos.y, 0.f, true };
        float simX = playerPos.x;

        for (int i = 0; i < BotPhysics::LOOKAHEAD_STEPS; i++) {
            bool press = jumpNow && i == 0;
            s = simulateStep(s, press, groundY);
            simX += BotPhysics::FORWARD_SPEED;

            CCRect playerRect(
                simX - BotPhysics::PLAYER_HALF_SIZE,
                s.y - BotPhysics::PLAYER_HALF_SIZE,
                BotPhysics::PLAYER_HALF_SIZE * 2.f,
                BotPhysics::PLAYER_HALF_SIZE * 2.f
            );

            for (auto& hz : hazards) {
                if (playerRect.intersectsRect(hz)) return true;
            }
        }
        return false;
    }
};
