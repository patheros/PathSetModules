/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"

Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelShiftyMod);
	p->addModel(modelIceTray);
	p->addModel(modelAstroVibe);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}