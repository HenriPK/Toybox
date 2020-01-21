#pragma once
#include <string.h>
#include <stdio.h>
#include <map>
#include <glm/glm.hpp>
#include <algorithm>
#include "any.h"
using namespace std;
/* For now, using a fixed value! */
#define MAX_ENTITIES 5000
#define MAX_SYSTEMS 10

/* Creating my own typenames for better understanding (and usage) of the code. */
using EntityUID = unsigned int;
using SystemUID = unsigned int;
using EntityType = unsigned int;
using ComponentType = string;
using var = any;

/* UTIL FUNCTIONS */
void log(string message)
{
	cout << message << endl;
}

void err(string message)
{
	cerr << message << endl;
}
/* END OF UTIL FUNCTIONS */

namespace tvx
{

	/* IMPORTANT NOTE:
		This struct's functionality is incomplete.
		It's going to be fixed on the next commit.

		Note the 'any' type? The 'component_sets' map
		holds ComponentSets but they're all different values
		because ComponentSets is a template class.
	*/
	struct ComponentHandler //Handles all the component sets of a scene
	{
	private:
		map<ComponentType, any*> component_sets;
	public:
		void add(ComponentType type, any set)
		{
			component_sets[type] = &set;
		}

		void remove(ComponentType type)
		{
			component_sets.erase(type);
		}

		template <class ComponentSet>
		ComponentSet* get_c(string type)
		{
			any return_value = component_sets[ComponentType(type)];
			assert(return_value.is<ComponentSet>());
			return return_value;
		}
	};

	//A single set handler, because there's only one *type* of entity. To give labels to entity, add the 'TypeTag' component.
	struct EntityHandler
	{
	private:
		//All the valied entity UIDs, densely packed for easy traversal.
		//Every time an entity is removed, the last entity fills the blank space.
		EntityUID entities[MAX_ENTITIES] = {0};
		size_t entity_count = 0;
		size_t next_id = 1;

		//If there's already an entity with that index, increase until there's not.
		//Sometimes you'll be in situations like this:
		//	entities = {0, 1, 4, 5, 6}
		//In this case, if next_id is, for example, 3, it's not a problem!
		void update_nid()
		{
			while(find(begin(entities), end(entities), next_id) != end(entities))
			{
				next_id++;
			}
		}
	public:
		EntityUID create()
		{
			//If the last index isn't free, so we've reached the entity limited
			//After all, it's densely packed data, there are no holes anywhere inside...
			if(entities[MAX_ENTITIES-1] != 0)
			{
				err("Toyvox Warning: ENTITY LIMIT REACHED");
				return 0;
			}

			entities[entity_count] = next_id;

			entity_count += 1;
			next_id += 1;
			update_nid();

			return next_id - 1;
		}

		//Note that here you use the index that create() gave you, not the entity UID!
		void destroy(EntityUID* entity)
		{
			size_t position = *find(begin(entities), end(entities), *entity);

			entities[position] = entities[entity_count];						//Move the last entity to this entity's place. Nice.
			entities[entity_count] = 0;

			if(*entity < next_id) 	//Set the next_id to fill in the hole next time create() is called.
				next_id = *entity;   //Of course you're not gonna change next_id if entity is bigger, let's not leave any holes behind.
			entity_count--;
			*entity=0;
		}
	};
	
	/*
					SYSTEM:
		An abstract class.
		Can do things to components.
		Affects all "objects" but only ever
		touches the components it needs to.
	*/
	class System
	{
	private:
		bool active_status;
	public:
		bool is_active(){return active_status;}
		void set_active(bool active){active_status = active;}
	};


	/* 
					SCENE:
		Has the handlers that handle all
		systems, components, entities.
		With it, you can make rooms/levels.

		You can have scenes talking to each
		other easily, with 'global variables'.
	*/

	class Scene
	{
	public:

		ComponentHandler* component_handler;	
		EntityHandler* entity_handler;

		Scene(char* title)
		{
			name = title;
		}

		~Scene()
		{}

		inline char* get_title()
		{
			return strdup(name);
		}

		EntityUID create_entity()
		{
			return entity_handler->create();
		}

		void destroy_entity(EntityUID* ent)
		{
			entity_handler->destroy(ent);
		}

		/* Register handlers (overloaded functions!) */
		void add_handler(ComponentHandler* x)
		{
			component_handler = x;
		}

		void add_handler(EntityHandler* x)
		{
			entity_handler = x;
		}

		/* Be sure to implement these functions whenever you create a new scene,
		or assign them to an existing function!!! */
		void (*on_load)();
		void (*every_tick)();
	private:
		char* name;
	};

	/* 					Game				 		*/
	/* 	The world basically contains which scene
		is the current. Also, some other 'global'
		settings.									*/

	struct Game
	{
	private:
		Scene* current_scene;
		string game_title;
		glm::ivec2 resolution;
	public:
		Scene get_current_scene(){return *current_scene;}
		void set_current_scene(Scene* new_scene)
		{
			current_scene = new_scene;
			new_scene->on_load();
		}

		string get_title(){return game_title;}
		void set_title(string title){game_title = title;}

		glm::ivec2 get_resolution(){return resolution;}
		void set_resolution(glm::ivec2 res){resolution = res;}

		Game()
		{}
	};
}