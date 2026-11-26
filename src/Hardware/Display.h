#pragma once

void drawHeader(const char* pageName);
void drawPanel(int x, int y, int w, int h);
void drawHomeButton();
void drawTile(int x, const char* label, const char* symbol, bool selected);
void drawHomeTiles();
void updateHomeSelection();
