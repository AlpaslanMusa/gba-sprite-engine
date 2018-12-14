//
// Created by Wouter Groeneveld on 09/08/18.
//

#include <libgba-sprite-engine/sprites/sprite_builder.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <algorithm>
#include <libgba-sprite-engine/background/text_stream.h>
#include <sstream>
#include "food_scene.h"
#include "avatar.h"
#include "bullet.h"
#include "bullet_data.h"

#define AVATAR_ROTATION_DIFF (128 * 6)
#define MAX_AMOUNT_OF_BULLETS 40
#define BULLET_COOLDOWN_START 20

FoodScene::FoodScene(const std::shared_ptr<GBAEngine> &engine) : Scene(engine) {}

std::vector<Background *> FoodScene::backgrounds() {
    return {};
}

std::vector<Sprite *> FoodScene::sprites() {
    std::vector<Sprite*> sprites;
    // TODO order is important for affine transformations because of affinity - those always last!
    // some kind of affine bug depending on the order of sprites added in OAM
    for(auto& b : bullets) {
        sprites.push_back(b->getSprite());
    }
    sprites.push_back(someBulletSprite.get());
    sprites.push_back(avatar.get());

    return sprites;
}

void FoodScene::removeBulletsOffScreen() {
    bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(), [](std::unique_ptr<Bullet> &s) { return s->isOffScreen(); }),
            bullets.end());
}

VECTOR FoodScene::rotateAround(VECTOR center, VECTOR point) {
    s32 centerx = center.x, centery = center.y;
    s32 defaultx = point.x, defaulty = point.y;

    s32 cos = lu_cos(avatarRotation) >> 4;
    s32 sin = lu_sin(avatarRotation) >> 4;

    // affine matriches are 8.8 fixed point numbers, so shift all input 8 spaces up and forth
    // possibilities: instead of between [-1.0, 1.0] it's between [-256, +256]
    // 90° rotation in inversed y-axis needs to flip sin sign
    return {
            ( cos * (defaultx - centerx) + sin * (defaulty - centery) + (centerx << 8)) >> 8,
            (-sin * (defaultx - centerx) + cos * (defaulty - centery) + (centery << 8)) >> 8};
}

void FoodScene::tick(u16 keys) {
    avatar->animateToFrame(0);
    bool allowedToShoot = false;

    if(bulletCooldown > 0) {
        bulletCooldown--;
    } else if(bulletCooldown == 0) {
        allowedToShoot = true;
    }

    removeBulletsOffScreen();
    TextStream::instance().setText(std::string("bullets: ") + std::to_string(bullets.size()), 1, 1);
    TextStream::instance().setText(std::string("cooldown: ") + std::to_string(bulletCooldown), 2, 1);

    TextStream::instance().setText(std::string("angle pa/pb: ") + std::to_string(avatar->getMatrix()->pa) + std::string("/") + std::to_string(avatar->getMatrix()->pb), 3, 1);
    TextStream::instance().setText(std::string("angle pc/pd: ") + std::to_string(avatar->getMatrix()->pc) + std::string("/") + std::to_string(avatar->getMatrix()->pd), 4, 1);

    if(keys & KEY_LEFT) {
        avatarRotation -= AVATAR_ROTATION_DIFF;
    } else if(keys & KEY_RIGHT) {
        avatarRotation += AVATAR_ROTATION_DIFF;
    }
    if((keys & KEY_A)) {
        avatar->animateToFrame(1);

        if(allowedToShoot && bullets.size() < MAX_AMOUNT_OF_BULLETS) {
            bulletCooldown = BULLET_COOLDOWN_START;
            bullets.push_back(createBullet());

            auto &b = bullets.at(bullets.size() - 1);
            auto destination = rotateAround(avatar->getCenter(), defaultBulletTarget);
            TextStream::instance().setText(std::string("shooting dest: ") + std::to_string(destination.x) + std::string(",") + std::to_string(destination.y), 16, 1);
            b->setDestination(destination);
        }
    }

    avatar->rotate(avatarRotation);

    for(auto &b : bullets) {
        b->tick();
    }
 }

 std::unique_ptr<Bullet> FoodScene::createBullet() {
    return std::unique_ptr<Bullet>(new Bullet(spriteBuilder
            ->withLocation(avatar->getX() + avatar->getWidth() / 2, avatar->getY() + avatar->getHeight() / 2)
            .buildWithDataOf(*someBulletSprite.get())));
}

void FoodScene::load() {
    foregroundPalette = std::unique_ptr<ForegroundPaletteManager>(new ForegroundPaletteManager(avatar_palette, sizeof(avatar_palette)));
    backgroundPalette = std::unique_ptr<BackgroundPaletteManager>(new BackgroundPaletteManager());

    spriteBuilder = std::unique_ptr<SpriteBuilder<Sprite>>(new SpriteBuilder<Sprite>);
    SpriteBuilder<AffineSprite> affineBuilder;

    // Doom avatar copyright iD Software
    avatar = affineBuilder.withData(avatar_data, sizeof(avatar_data))
            .withSize(SIZE_64_64)
            .withAnimated(2, 3)
            .withLocation(GBA_SCREEN_WIDTH / 2 - 32, GBA_SCREEN_HEIGHT / 2 - 32)
            .buildPtr();
    avatar->stopAnimating();
    avatarRotation = 0;

    someBulletSprite = spriteBuilder->withData(bullet_data, sizeof(bullet_data))
            .withSize(SIZE_16_16)
            .withLocation(GBA_SCREEN_WIDTH + 20, GBA_SCREEN_HEIGHT + 20)
            .buildPtr();
    bulletCooldown = BULLET_COOLDOWN_START;

    // rotation of a point on a circle within the resolution means our radius should be big enough
    defaultBulletTarget = { GBA_SCREEN_WIDTH / 2, GBA_SCREEN_HEIGHT + (GBA_SCREEN_WIDTH / 2) - avatar->getCenter().y + 40};
}