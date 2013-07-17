#include "entity.h"

using namespace sf;

Entity::Entity(EntityShape shape0, Engine *engine0) {
	shape = shape0;
	position.x = 0; position.y = 0;
	isKilledFlag = false;
	engine = engine0;
	UID = ++globalUID;
}
Entity::~Entity() {
	engine = NULL;
}

Uint32 Entity::globalUID = 0;
Uint32 Entity::getUID(void) const {return UID;}
void Entity::setUID(Uint32 uid) {UID = uid;}
void Entity::useUpperUID(void) {
	globalUID |= 0x80000000;
}
