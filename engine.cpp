#include <math.h>

#include "engine.h"
#include "entity.h"
#include "geometry.h"
#include "engine_event.h"
#include "misc.h"
#include "wall.h"
#include "player.h"
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>
#include <SFML/Graphics/Text.hpp>
#include <stdio.h>
#include "commands.h"

using namespace sf;

static bool interacts(Engine *engine, MoveContext &ctx, Entity *a, Entity *b);

void Engine::play(void) {
	while (step()) {
		Event e;
		while (window.pollEvent(e)) {
			if (e.type == Event::Closed) {
				quit();
			} else if (e.type == Event::KeyPressed) {
				if (e.key.code == Keyboard::Escape) quit();
			} else if (e.type == Event::JoystickConnected) {
				addPlayer(0, e.joystickConnect.joystickId);
			} else if (e.type == Event::JoystickDisconnected) {
				Player *pl = getPlayerByJoystickId(e.joystickConnect.joystickId);
				if (pl) {destroy(pl);destroy_flagged();}
			} else if (KeymapController::maybeConcerned(e)) {
				controller_activity(e);
			}
		}
	}
}

void Engine::controller_activity(Event &e) {
	for(EntitiesIterator it=entities.begin(); it != entities.end(); it++) {
		Player *pl = dynamic_cast<Player*>(*it);
		if (!pl) continue;
		KeymapController *c = dynamic_cast<KeymapController*>(pl->getController());
		if (c) {
			int ojoyid = 0;
			if (c->isConcerned(e, ojoyid)) return;
			continue;
		}
	}
	/* this key event is not owned by a player, have a look at controller templates */
	std::vector<KeymapController*> &cd = cdef.forplayer;
	for(unsigned i = 0; i < cd.size(); i++) {
		int ojoyid = 0;
		if (cd[i] && cd[i]->isConcerned(e, ojoyid)) {
			addPlayer(i, ojoyid);
			break;
		}
	}
}
Player *Engine::getPlayerByJoystickId(int joyid) {
	for(EntitiesIterator it=entities.begin(); it != entities.end(); it++) {
		Player *pl = dynamic_cast<Player*>(*it);
		if (!pl) continue;
		KeymapController *c = dynamic_cast<KeymapController*>(pl->getController());
		if (c) {
			if (c->getJoystickId() == joyid) return pl;
			continue;
		}
		JoystickController *j = dynamic_cast<JoystickController*>(pl->getController());
		if (j) {
			if (j->getJoystickId() == joyid) return pl;
		}
	}
	return NULL;
}
void Engine::addPlayer(unsigned cid, int joyid) {
	if (cid >= cdef.forplayer.size()) return;
	if (cid == 0 && joyid ==  -1) {
		/* search a free joystick */
		for(unsigned i=0; i < Joystick::Count; i++) {
			if (Joystick::isConnected(i) && !getPlayerByJoystickId(i)) {
				joyid = i;
				break;
			}
		}
	}
	if (!cdef.forplayer[cid]) {
		fprintf(stderr, "Error: no controller %d found for new player\n", cid);
		return;
	}
	add(new Player(cdef.forplayer[cid]->clone(joyid), this));
}
Entity *Engine::getMapBoundariesEntity() {
	return map_boundaries_entity;
}
TextureCache *Engine::getTextureCache(void) const {
	return &texture_cache;
}
Vector2d Engine::map_size(void) {
	Vector2u sz = window.getSize();
	return Vector2d(sz.x, sz.y);
}
Engine::EntitiesIterator Engine::begin_entities() {
	return entities.begin();
}
Engine::EntitiesIterator Engine::end_entities() {
	return entities.end();
}
Engine::Engine() {
	first_step = true;
	must_quit = false;
	window.create(VideoMode(1920,1080), "Tank window", Style::Default);
	window.setVerticalSyncEnabled(true);
	window.clear(Color::White);
	score_font.loadFromFile("/usr/share/fonts/truetype/droid/DroidSans.ttf");

	load_texture(background, background_texture, "sprites/dirt.jpg");
	Vector2d sz = map_size();
	background.setTextureRect(IntRect(0,0,sz.x,sz.y));
	map_boundaries_entity = new Wall(0,0,sz.x,sz.y, NULL, this);
	add(map_boundaries_entity);
	load_keymap(cdef, "keymap.json");
}
Engine::~Engine() {
	for(EntitiesIterator it=entities.begin(); it != entities.end(); ++it) {
		delete (*it);
	}
	window.close();
}
void Engine::seekCollisions(Entity *entity) {
	int i=100;
	bool retry = false;
	do {
	for(EntitiesIterator it=entities.begin(); it != entities.end(); ++it) {
		Entity *centity = (*it);
		Segment vect;
		vect.pt1 = entity->position;
		vect.pt2 = entity->position;
		MoveContext ctx(IT_GHOST, vect);
		if (interacts(this, ctx, entity, centity)) {
			CollisionEvent e;
			e.first = entity;
			e.second = centity;
			e.interaction = IT_GHOST;
			broadcast(&e);
			if (e.retry) retry = true;
		}
	}
	} while(retry && --i > 0);
}
void Engine::add(Entity *entity) {
	entity->setEngine(this);
	entities.push_back(entity);
	/* signal spawn-time collisions */
	seekCollisions(entity);
}
void Engine::destroy(Entity *entity) { /* Removes entity from engine and deletes the underlying object */
	entity->setKilled();
}
void Engine::broadcast(EngineEvent *event) {
	/* assume iterators may be invalidated during event processing, because new items may be added */
	/* note that entities may be set to killed, but cannot be removed from the list (they are only flagged) */

	for(size_t i=0; i < entities.size(); ++i) {
		Entity *entity = entities[i];
		if (!entity->isKilled()) entity->event_received(event);
	}
}
void Engine::quit(void) {
	must_quit = true;
}
bool Engine::step(void) {
	if (must_quit) return false;
	if (first_step) {clock.restart();first_step=false;}
	draw();
	compute_physics();
	destroy_flagged();
	return !must_quit;
}
void draw_score(RenderTarget &target, Font &ft, int score, Color color, Vector2d pos) {
	Text text;
	char sscore[256];
	sprintf(sscore, "%d", score);
	text.setCharacterSize(128);
	text.setString(sscore);
	text.setColor(color);
	text.setPosition(Vector2f(pos.x, pos.y));
	text.setFont(ft);

	target.draw(text);
}
void Engine::draw(void) {
	Vector2d scorepos;
	scorepos.x = 16;
	scorepos.y = 16;
	window.clear();
	window.draw(background);
	for(EntitiesIterator it=entities.begin(); it != entities.end(); ++it) {
		(*it)->draw(window);

		if (Player *pl = dynamic_cast<Player*>((*it))) {
			draw_score(window, score_font, pl->getScore(), pl->getColor(), scorepos);
			scorepos.x += 384+16;
		}
	}
	window.display();
}
static double getEntityRadius(Entity *a) {
	return a->getSize().x/2;
}
static DoubleRect getEntityBoundingRectangle(Entity *a) {
	Vector2d pos = a->position;
	Vector2d size = a->getSize();
	DoubleRect r;
	r.left = pos.x;
	r.top  = pos.y;
	r.width  = size.x;
	r.height = size.y;
	return r;
}
static Circle getEntityCircle(Entity *a) {
	Circle circle;
	circle.filled = true;
	circle.center = a->position;
	circle.radius = getEntityRadius(a);
	return circle;
}
/* Note: Dynamic entities must be circle-shaped */
static bool interacts(Engine *engine, MoveContext &ctx, Entity *a, Entity *b) {
	if (a->shape == SHAPE_CIRCLE && b->shape == SHAPE_RECTANGLE) {
		GeomRectangle gr;
		gr.filled = (b != engine->getMapBoundariesEntity());
		gr.r = getEntityBoundingRectangle(b);
		return moveCircleToRectangle(getEntityRadius(a), ctx, gr);
	} else if (a->shape == SHAPE_CIRCLE && b->shape == SHAPE_CIRCLE) {
		return moveCircleToCircle(getEntityRadius(a), ctx, getEntityCircle(b));
	} else {
		return false;
	}
}
bool moveCircleToRectangle(double radius, MoveContext &ctx, const DoubleRect &r);
bool moveCircleToCircle(double radius, MoveContext &ctx, const Circle &colli);

static bool quasi_equals(double a, double b) {
	return fabs(a-b) <= 1e-3*(fabs(a)+fabs(b));
}
void Engine::compute_physics(void) {
	Int64 tm = clock.getElapsedTime().asMicroseconds();
	if (tm == 0) return;
	clock.restart();
	for(EntitiesIterator it=entities.begin(); it != entities.end(); ++it) {
		Entity *entity = (*it);
		if (entity->isKilled()) continue;
		Vector2d movement = entity->movement(tm);
		Vector2d old_speed = movement;
		old_speed.x /= tm;
		old_speed.y /= tm;
		Segment vect;
		vect.pt1 = entity->position;
		vect.pt2 = vect.pt1 + movement;
		if ((fabs(movement.x)+fabs(movement.y)) < 1e-4) continue;
		MoveContext ctx(IT_GHOST, vect);
		for(int pass=0; pass < 2; ++pass)
		for (EntitiesIterator itc=entities.begin(); itc != entities.end(); ++itc) {
			Entity *centity = *itc;
			if (centity->isKilled()) continue;
			if (entity->isKilled()) break;
			if (centity == entity) continue;
			ctx.interaction = IT_GHOST;
			MoveContext ctxtemp = ctx;
			if (interacts(this, ctxtemp, entity, centity)) {
				if (entity->isKilled() || centity->isKilled()) continue;
				CollisionEvent e;
				e.type   = COLLIDE_EVENT;
				e.first  = entity;
				e.second = centity;
				e.interaction = IT_GHOST; /* default interaction type */
				broadcast(&e); /* should set e.interaction */
				ctx.interaction = e.interaction;
				if (pass == 1 && ctx.interaction != IT_GHOST) {
					/* On second interaction in the same frame, cancel movement */
					ctx.vect.pt2 = ctx.vect.pt1 = entity->position;
					break;
				}
				interacts(this, ctx, entity, centity);
			}
		}
		if (entity->isKilled()) continue;
		vect = ctx.vect;
		CompletedMovementEvent e;
		e.type = COMPLETED_MOVEMENT_EVENT;
		e.entity = entity;
		e.position = vect.pt2;
		Vector2d new_speed = ctx.nmove;
		new_speed.x /= tm;
		new_speed.y /= tm;
		if (quasi_equals(old_speed.x, new_speed.x) && quasi_equals(old_speed.y, new_speed.y)) {
			e.new_speed = old_speed;
			e.has_new_speed = false;
		} else {
			e.new_speed = new_speed;
			e.has_new_speed = true;
		}
		broadcast(&e);
	}
}
void Engine::destroy_flagged(void) {
	bool some_got_deleted;
	do {
		some_got_deleted = false;
		for(size_t i=0; i < entities.size(); ++i) {
			Entity *entity = entities[i];
			if (entity->isKilled()) {
				some_got_deleted = true;
				EntityDestroyedEvent e;
				e.type = ENTITY_DESTROYED_EVENT;
				e.entity = entity;
				broadcast(&e);
				entities.erase(entities.begin()+i);
				delete entity;
				--i;
			}
		}
	} while(some_got_deleted);
#if 0
	if (some_got_deleted) fprintf(stderr, "[now, I'm going to physically destroy things]\n");
	for(size_t i=0; i < entities.size(); ++i) {
		Entity *entity = entities[i];
		if (entity->isKilled()) {
			entities.erase(entities.begin()+i);
			delete entity;
			--i;
		}
	}
	if (some_got_deleted) fprintf(stderr, "[/physically destroyed things]\n");
#endif
}

