// Read-only triangle intersection probe for the rendered, skinned geometry.
struct ContactAudit {
    struct V {double x=0,y=0,z=0;V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};}V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};}V operator*(double s)const{return{x*s,y*s,z*s};}};
    static V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
    static double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
    struct Box {V lo={1e30,1e30,1e30},hi={-1e30,-1e30,-1e30};void add(V p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}bool overlaps(const Box& b)const{return lo.x<=b.hi.x&&hi.x>=b.lo.x&&lo.y<=b.hi.y&&hi.y>=b.lo.y&&lo.z<=b.hi.z&&hi.z>=b.lo.z;}};
    struct Triangle {V p[3];Box box;std::string bone;};
    struct Node {Box box;int left=-1,right=-1,begin=0,end=0;};
    std::vector<Triangle> weapon;
    std::vector<Node> nodes;
    std::vector<XMFLOAT3> points;
    std::map<std::string,int> bones;
    int count=0;
    static bool edge(V a,V b,const Triangle& t,V& hit){
        auto d=b-a,e1=t.p[1]-t.p[0],e2=t.p[2]-t.p[0],h=cross(d,e2);double determinant=dot(e1,h);if(std::abs(determinant)<1e-15)return false;
        double inverse=1/determinant;auto s=a-t.p[0];double u=inverse*dot(s,h);if(u<-1e-7||u>1.0000001)return false;auto q=cross(s,e1);double v=inverse*dot(d,q);if(v<-1e-7||u+v>1.0000001)return false;
        double at=inverse*dot(e2,q);if(at<=.000001||at>=.999999)return false;hit=a+d*at;return true;
    }
    static bool intersects(const Triangle& a,const Triangle& b,V& hit){if(!a.box.overlaps(b.box))return false;for(int i=0;i<3;i++)if(edge(a.p[i],a.p[(i+1)%3],b,hit)||edge(b.p[i],b.p[(i+1)%3],a,hit))return true;return false;}
    int build(int begin,int end){
        Node node;node.begin=begin;node.end=end;for(int i=begin;i<end;i++)for(auto p:weapon[i].p)node.box.add(p);int index=(int)nodes.size();nodes.push_back(node);
        if(end-begin>8){V size=node.box.hi-node.box.lo;int axis=size.y>size.x?1:0;if(size.z>(axis?size.y:size.x))axis=2;int mid=(begin+end)/2;std::nth_element(weapon.begin()+begin,weapon.begin()+mid,weapon.begin()+end,[axis](auto& a,auto& b){auto c=a.box.lo+a.box.hi,d=b.box.lo+b.box.hi;return axis==0?c.x<d.x:axis==1?c.y<d.y:c.z<d.z;});nodes[index].left=build(begin,mid);nodes[index].right=build(mid,end);}
        return index;
    }
    bool query(int index,const Triangle& t,V& hit)const{
        auto& node=nodes[index];if(!node.box.overlaps(t.box))return false;if(node.left>=0)return query(node.left,t,hit)||query(node.right,t,hit);
        for(int i=node.begin;i<node.end;i++)if(intersects(t,weapon[i],hit))return true;return false;
    }
    void check(Scene& scene,Entity instance){
        weapon.clear();nodes.clear();points.clear();bones.clear();count=0;std::vector<Triangle> skin;
        for(size_t oi=0;oi<scene.objects.GetCount();oi++){
            auto e=scene.objects.GetEntity(oi);bool gun=e==instance||scene.Entity_IsDescendant(e,instance);auto mesh=scene.meshes.GetComponent(scene.objects[oi].meshID);if(!mesh)continue;
            auto rig=scene.armatures.GetComponent(mesh->armatureID);if(!gun&&(!rig||std::find(rig->boneCollection.begin(),rig->boneCollection.end(),scene.Entity_FindByName("hand_l"))==rig->boneCollection.end()))continue;
            auto world=XMLoadFloat4x4(&scene.matrix_objects[oi]);std::vector<V> verts;verts.reserve(mesh->vertex_positions.size());
            for(uint32_t v=0;v<mesh->vertex_positions.size();v++){auto p=rig&&!rig->boneData.empty()?SkinVertex(*mesh,*rig,v):XMLoadFloat3(&mesh->vertex_positions[v]);auto f=ArmFit::f3(XMVector3TransformCoord(p,world));verts.push_back({f.x,f.y,f.z});}
            auto& triangles=gun?weapon:skin;
            for(size_t i=0;i+2<mesh->indices.size();i+=3){Triangle t;for(int k=0;k<3;k++){t.p[k]=verts[mesh->indices[i+k]];t.box.add(t.p[k]);}
                if(!gun){uint32_t v=mesh->indices[i];if(v<mesh->vertex_boneindices.size()){auto& weights=mesh->vertex_boneweights[v];auto& ids=mesh->vertex_boneindices[v];int slot=0;for(int k=1;k<4;k++)if((&weights.x)[k]>(&weights.x)[slot])slot=k;uint32_t joint=(&ids.x)[slot];if(joint<rig->boneCollection.size())if(auto name=scene.names.GetComponent(rig->boneCollection[joint]))t.bone=name->name;}}
                triangles.push_back(std::move(t));
            }
        }
        if(weapon.empty())return;nodes.reserve(weapon.size()/3);build(0,(int)weapon.size());
        for(auto& t:skin){V hit;if(query(0,t,hit)){count++;bones[t.bone]++;if(points.size()<500)points.push_back({(float)hit.x,(float)hit.y,(float)hit.z});}}
    }
};
