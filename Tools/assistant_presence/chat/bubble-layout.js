'use strict';
function bubbleLayout(center,work,size={},diameter=220){
 size=size||{};
 const pad=12,gap=12,r=diameter/2;
 const width=Math.min(Math.max(320,Math.round(size.width||400)),Math.max(280,work.width-pad*2));
 const height=Math.min(Math.max(320,Math.round(size.height||490)),Math.max(260,work.height-pad*2));
 const above=center.y-work.y>work.height/2;
 const x=Math.round(Math.max(work.x+pad,Math.min(work.x+work.width-width-pad,center.x-width+40)));
 const preferred=above?center.y-r-gap-height:center.y+r+gap;
 const y=Math.round(Math.max(work.y+pad,Math.min(work.y+work.height-height-pad,preferred)));
 return {bounds:{x,y,width,height},tail:{x:Math.max(28,Math.min(width-28,center.x-x)),edge:above?'bottom':'top'}};
}
module.exports={bubbleLayout};
