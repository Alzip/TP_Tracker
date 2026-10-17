#include "map_interaction.hpp"
#include <stdexcept>
#include <iostream>
#include <limits>
using namespace tracker;
void require(bool v) { if(!v) throw std::runtime_error("map interaction regression"); }
bool near(float a,float b) { return std::abs(a-b)<0.01f; }
int main() {
 MapHitTargets t; t.clear({0,0,200,150}); t.add(50,50,12,12,"Chest"); t.add(90,50,12,12,"Rupee");
 require(t.hit({50,50})=="Chest" && t.hit({90,50})=="Rupee");
 require(t.hit({65,50}).empty() && t.hit({50,75}).empty()); // not merely nearby
 t.add(50,50,16,16,""); require(t.hit({50,50}).empty()); // aggregate marker blocks underlying check
 t.add(220,50,12,12,"Offscreen"); require(t.hit({220,50}).empty());
 t.clear({0,0,200,150}); require(t.hit({50,50}).empty());
 t.add(10,10,8,8,"Behind hint"); t.add(90,10,90,10,""); require(t.hit({10,10}).empty());
 t.clear({0,0,200,150}); t.add(10,10,8,8,"Behind hint");
 t.setHint(MapHitRect{0,0,180,20}); require(t.hit({10,10}).empty());
 t.setHint(std::nullopt); require(t.hit({10,10})=="Behind hint"); // hiding does not clear checks or retain an invisible blocker
 require(t.targets.size()==1);
 t.clear({0,0,200,150});
 for(bool mirrored:{false,true}) {
  FieldMapTransform map{20,10,100,mirrored};
  auto screen=map.screen(40,30); t.add(screen.x,screen.y,12,12,"Chest");
  require(t.hit(screen)=="Chest"); t.clear({0,0,200,150});
 }
 int mapOwner=0,otherOwner=0;
 MapInspectionFrame frame;
 for(bool dungeon:{false,true}) {
  require(frame.begin(&mapOwner,&mapOwner,dungeon,true,"stage",{0,0,200,150},{50,50}));
  frame.targets.add(50,50,12,12,"Chest");
  for(bool visible:{true,false,true,false}) {
   frame.targets.setHint(visible ? std::optional<MapHitRect>{{0,0,100,20}} : std::nullopt);
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)=="Chest");
   require(!frame.begin(&otherOwner,&mapOwner,!dungeon,false,"other",{},{}));
   require(frame.active(&mapOwner,dungeon,true,"stage") && frame.targets.hit(frame.cursor)=="Chest");
  }
  require(!frame.active(nullptr,dungeon,true,"stage"));
  require(!frame.active(&mapOwner,dungeon,false,"stage"));
  require(!frame.active(&mapOwner,dungeon,true,"different scene"));
  frame.targets.panel=MapHitRect{40,40,60,60}; require(frame.targets.hit({50,50}).empty());
  frame.targets.panel.reset(); require(frame.targets.hit({50,50})=="Chest");
  require(frame.begin(&mapOwner,&mapOwner,dungeon,false,"stage",{},{}));
  require(!frame.owner && frame.targets.targets.empty());
 }
 for(float screenWidth:{608.f,854.f,1120.f}) {
  MapHitRect screen{0,0,screenWidth,480},map{274,60,600,395};
  const auto panel=dungeonSummaryBounds(map,screen);
  require(panel.left>=0 && panel.right<=screen.right && panel.top>=104 && panel.bottom<screen.bottom);
  require(near(panel.right-panel.left,112) && near(panel.bottom-panel.top,80));
  if(screenWidth==608) require(panel.left>map.left && panel.right<map.right);
  else require(panel.left>map.right);
 }
 MapHitRect game{-123,0,731,480};
 auto center=mapMousePoint(960,540,1920,1080,1920,1080,1920,1080,game);
 require(center && near(center->x,304) && near(center->y,240));
 auto dpi=mapMousePoint(400,300,800,600,1600,1200,1920,1080,game);
 require(dpi && near(dpi->x,304) && near(dpi->y,240));
 auto quarter=mapMousePoint(200,300,800,600,1600,1200,1920,1080,game);
 require(quarter && near(quarter->x,90.5f));
 require(!mapMousePoint(20,20,800,600,1600,1200,1920,1080,game)); // letterbox
 require(!mapMousePoint(0,0,0,600,1600,1200,1920,1080,game));
 require(!mapMousePoint(std::numeric_limits<float>::quiet_NaN(),0,800,600,1600,1200,1920,1080,game));
 std::cout << "Map icon geometry, mirrored hit tests and DPI/letterbox mapping passed\n";
}
