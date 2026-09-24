#include "PlayerRenderer.h"
#include "EntityRenderDispatcher.h"
#include "../../Options.h"
#include "../Textures.h"
#include "../../../world/entity/player/Player.h"
#include "../../../world/level/Level.h"
#include "../../../world/item/ArmorItem.h"

static const std::string armorFilenames[10] = {
	"armor/cloth_1.png",	"armor/cloth_2.png",
	"armor/chain_1.png",	"armor/chain_2.png",
	"armor/iron_1.png",		"armor/iron_2.png",
	"armor/diamond_1.png",	"armor/diamond_2.png",
	"armor/gold_1.png",		"armor/gold_2.png",
};

PlayerRenderer::PlayerRenderer( HumanoidModel* humanoidModel, float shadow )
:	super(humanoidModel, shadow),
	playerModel64(humanoidModel),
	playerModelSlim(new HumanoidModel(0, 0, 64, 64, true)),
	playerModel32(new HumanoidModel(0, 0, 64, 32)),
	armorParts1(new HumanoidModel(1.0f, 0, 64, 32)),
	armorParts2(new HumanoidModel(0.5f, 0, 64, 32))
{
	// Keep both layouts resident.  The appropriate one is selected immediately
	// before third-person and first-person player rendering.
	model = playerModel32;
	humanoidModel = playerModel32;
}

PlayerRenderer::~PlayerRenderer() {
	// MobRenderer must not delete either model: this class owns both of them.
	model = NULL;
	delete playerModel32;
	delete playerModel64;
	delete playerModelSlim;
	delete armorParts1;
	delete armorParts2;
}

bool PlayerRenderer::isModernPlayerSkin(Mob* mob) {
	const std::string textureName = mob->getTexture();

	// local.png is the documented import target and accepts only the modern
	// 64x64 layout.  Do not depend on the transient upload cache for it.
	if (textureName == "skins/local.png") return true;

	TextureId id = entityRenderDispatcher->textures->loadTexture(textureName);
	if (!Textures::isTextureIdValid(id)) return false;
	const TextureData* data = entityRenderDispatcher->textures->getTemporaryTextureData(id);
	return data && data->w == 64 && data->h == 64;
}

void PlayerRenderer::setModernSkin(Mob* mob) {
	HumanoidModel* wanted = playerModel32;
	if (isModernPlayerSkin(mob)) {
		wanted = (entityRenderDispatcher->options && entityRenderDispatcher->options->slimSkin) ? playerModelSlim : playerModel64;
	}
	if (model != wanted || humanoidModel != wanted) {
		model = wanted;
		humanoidModel = wanted;
	}
}

void PlayerRenderer::render(Entity* mob, float x, float y, float z, float rot, float a) {
	setModernSkin((Mob*)mob);
	HumanoidMobRenderer::render(mob, x, y, z, rot, a);
}

void PlayerRenderer::setupPosition( Entity* mob, float x, float y, float z ) {
	Player* player = (Player*) mob;
	if(player->isAlive() && player->isSleeping()) {
		return super::setupPosition(mob, x + player->bedOffsetX, y + player->bedOffsetY, z + player->bedOffsetZ);
	}
	return super::setupPosition(mob, x, y, z);
}

void PlayerRenderer::setupRotations( Entity* mob, float bob, float bodyRot, float a ) {
	Player* player = (Player*) mob;
	if(player->isAlive() && player->isSleeping()) {
		glRotatef(player->getSleepRotation(), 0, 1, 0);
		glRotatef(getFlipDegrees(player), 0, 0, 1);
		glRotatef(270, 0, 1, 0);
		return;
	}
	super::setupRotations(mob, bob, bodyRot, a);
}

void PlayerRenderer::renderName( Mob* mob, float x, float y, float z ){
	//@todo: figure out how to handle HideGUI
	if (mob != entityRenderDispatcher->cameraEntity && mob->level->adventureSettings.showNameTags) {
		renderNameTag(mob, ((Player*)mob)->name, x, y, z, 32);
	}
}

int PlayerRenderer::prepareArmor(Mob* mob, int layer, float a) {
	Player* player = (Player*) mob;

	ItemInstance* itemInstance = player->getArmor(layer);
	if (!ItemInstance::isArmorItem(itemInstance))
		return -1;

	ArmorItem* armorItem = (ArmorItem*) itemInstance->getItem();
	int fnIndex = (armorItem->modelIndex + armorItem->modelIndex) + (layer == 2 ? 1 : 0);
	bindTexture(armorFilenames[fnIndex]);

	HumanoidModel* armor = layer == 2 ? armorParts2 : armorParts1;

	armor->head.visible = layer == 0;
	//armor.hair.visible = layer == 0;
	armor->body.visible = layer == 1 || layer == 2;
	armor->arm0.visible = layer == 1;
	armor->arm1.visible = layer == 1;
	armor->leg0.visible = layer == 2 || layer == 3;
	armor->leg1.visible = layer == 2 || layer == 3;

	setArmor(armor);

	/*if (itemInstance.isEnchanted())
		return 15; */

	return 1;
}

void PlayerRenderer::onGraphicsReset() {
	if (playerModel32) playerModel32->onGraphicsReset();
	if (playerModel64) playerModel64->onGraphicsReset();
	if (playerModelSlim) playerModelSlim->onGraphicsReset();

	if (armorParts1) armorParts1->onGraphicsReset();
	if (armorParts2) armorParts2->onGraphicsReset();

	super::onGraphicsReset();
}
