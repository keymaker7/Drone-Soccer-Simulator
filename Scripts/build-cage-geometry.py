"""Original 42-cell geodesic cage, not manufacturer CAD. Units are centimetres."""
from itertools import combinations
import json
import math
from pathlib import Path

def add(a,b):return tuple(x+y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):return tuple(x/math.sqrt(dot(a,a)) for x in a)
def solid(a,b,c):return 2*math.atan2(abs(dot(a,cross(b,c))),1+dot(a,b)+dot(b,c)+dot(c,a))

def generate():
    g=(1+math.sqrt(5))/2
    vertices=[unit(v) for v in [(0,s,t*g) for s in (-1,1) for t in (-1,1)]+[(s,t*g,0) for s in (-1,1) for t in (-1,1)]+[(t*g,0,s) for s in (-1,1) for t in (-1,1)]]
    distance=lambda a,b:sum((x-y)**2 for x,y in zip(a,b))
    edge=min(distance(a,b) for a,b in combinations(vertices,2))
    triangles=[f for f in combinations(range(12),3) if all(abs(distance(vertices[a],vertices[b])-edge)<1e-8 for a,b in combinations(f,2))]
    midpoints={}
    def middle(a,b):
        key=tuple(sorted((a,b)))
        if key not in midpoints:midpoints[key]=len(vertices);vertices.append(unit(add(vertices[a],vertices[b])))
        return midpoints[key]
    refined=[]
    for a,b,c in triangles:
        ab,bc,ca=middle(a,b),middle(b,c),middle(c,a)
        refined.extend([(a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)])
    nodes=[unit(add(add(vertices[a],vertices[b]),vertices[c])) for a,b,c in refined]
    cells=[];edges=set();areas=[]
    for i,normal in enumerate(vertices):
        tangent=unit(cross(normal,(0,0,1) if abs(normal[2])<.9 else (0,1,0)))
        up=cross(normal,tangent)
        adjacent=[j for j,f in enumerate(refined) if i in f]
        adjacent.sort(key=lambda j:math.atan2(dot(nodes[j],up),dot(nodes[j],tangent)))
        area=sum(solid(normal,nodes[a],nodes[b]) for a,b in zip(adjacent,adjacent[1:]+adjacent[:1]))*20**2
        cells.append(adjacent);areas.append(area)
        for a,b in zip(adjacent,adjacent[1:]+adjacent[:1]):edges.add(tuple(sorted((a,b))))
    result=dict(design='Original Class40 42-cell geodesic cage, STRIKER V3 silhouette reference only',
                radius_cm=19.65,rod_diameter_cm=.7,nodes=[[round(x*19.65,7) for x in n] for n in nodes],
                edges=[list(e) for e in sorted(edges)],cells=cells,cell_opening_upper_bounds_cm2=[round(a,5) for a in areas],
                max_opening_upper_bound_cm2=max(areas),note='Ideal spherical surface areas at 20cm radius, without subtracting rods. Not a physical compliance certification.')
    assert len(nodes)==80 and len(edges)==120 and len(cells)==42 and max(areas)<150
    path=Path(__file__).resolve().parents[1]/'Content/Data/cage-geodesic.json'
    path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(nodes=len(nodes),edges=len(edges),cells=len(cells),max_opening_cm2=max(areas))))

if __name__=='__main__':generate()
