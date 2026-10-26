"""Original hard-surface equipment assets. Run with Blender 4.5 LTS --background.

All modelling coordinates are metres. Dimensions are simulator assumptions, not
manufacturer CAD or measured STRIKER V3 production specifications.
"""
import bpy
from mathutils import Vector
import math
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'ArtSource'/'Airframe'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=1
M={}

def material(name,color,metal=0,rough=.45):
    m=bpy.data.materials.new('IW_'+name);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
    M[name]=m;return m

material('Carbon',(.045,.05,.055),.12,.35)
material('Anodized',(.075,.082,.095),.85,.29)
material('Steel',(.5,.56,.6),.95,.24)
material('Copper',(.64,.22,.073),.95,.25)
material('Gold',(.78,.5,.12),.9,.22)
material('PCB',(.015,.09,.052),.05,.42)
material('Trace',(.025,.19,.095),.15,.35)
material('Rubber',(.012,.015,.02),0,.72)
material('Black',(.018,.024,.031),.05,.39)
material('White',(.72,.75,.68),0,.43)
material('Red',(.43,.012,.008),0,.42)
material('Yellow',(.8,.49,.022),0,.48)
material('Label',(.75,.78,.76),0,.57)
material('Ink',(.008,.012,.016),0,.6)
# Original dark woven texture. The older greyscale detail texture is a height
# pattern and is too bright when interpreted directly as carbon base colour.
tex=bpy.data.images.new('IW_DarkCarbonWeave',width=256,height=256)
pixels=[]
for y in range(256):
    for x in range(256):
        across=((x//16+y//16)%2)==0
        fibre=(x if across else y)%16
        v=.028+.018*math.sin(math.pi*fibre/16)**2+.003*math.sin((x+y)*2.1)
        pixels.extend((v*.9,v*.95,v,1))
tex.pixels=pixels;tex.pack()
nodes=M['Carbon'].node_tree.nodes;links=M['Carbon'].node_tree.links
t=nodes.new('ShaderNodeTexImage');t.image=tex
links.new(t.outputs['Color'],nodes.get('Principled BSDF').inputs['Base Color'])

def finish(o,name,mat,bevel=0,smooth=False):
    o.name=name;o.data.materials.append(M[mat])
    if bevel:
        mod=o.modifiers.new('Manufactured edge radius','BEVEL');mod.width=bevel;mod.segments=3
    if o.type=='MESH' and smooth:
        for p in o.data.polygons:p.use_smooth=True
    return o

def box(name,p,size,mat,bevel=.0003,rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if rotation:o.rotation_euler=rotation
    return finish(o,name,mat,bevel)

def cyl(name,p,r,h,mat,vertices=48,bevel=.00015,rotation=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=h,location=p)
    o=bpy.context.object
    if rotation:o.rotation_euler=rotation
    return finish(o,name,mat,bevel,True)

def ring(name,p,ri,ro,h,mat,n=64,start=0,end=2*math.pi):
    verts=[];faces=[]
    for z in [-h/2,h/2]:
        for r in [ri,ro]:
            for i in range(n+1):
                a=start+(end-start)*i/n;verts.append((p[0]+r*math.cos(a),p[1]+r*math.sin(a),p[2]+z))
    stride=n+1
    for i in range(n):
        for a,b in [(0,1),(1,3),(3,2),(2,0)]:faces.append((a*stride+i,a*stride+i+1,b*stride+i+1,b*stride+i))
    faces.extend([(0,stride,3*stride,2*stride),(n,2*stride-1,4*stride-1,3*stride-1)])
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o)
    return finish(o,name,mat,.00012,True)

def wire(name,points,radius,mat):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=8
    curve.bevel_depth=radius;curve.bevel_resolution=3
    s=curve.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    for p,co in zip(s.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(o);o.data.materials.append(M[mat]);return o

def screw(p,r=.0015):
    ring('Socket screw',p,r*.43,r,.0013,'Steel',24)
    cyl('Dark socket recess',(p[0],p[1],p[2]-.0003),r*.44,.0003,'Black',6,0)

def text(name,body,p,size,mat,rotation=(0,0,0)):
    c=bpy.data.curves.new(name,'FONT');c.body=body;c.size=size;c.extrude=.000005;c.align_x='CENTER';c.align_y='CENTER'
    o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o);o.location=p;o.rotation_euler=rotation;c.materials.append(M[mat]);return o

def pcb(name,p,size):
    o=box(name,p,(size[0],size[1],.0016),'PCB',.0005)
    for x in [-.01525,.01525]:
        for y in [-.01525,.01525]:ring('PCB plated mount',(p[0]+x,p[1]+y,p[2]+.00085),.00105,.00165,.00009,'Gold',24)
    return o

# Carbon arm plates and layered central stack.
for z in [-.0016,.0016]:
    box('Carbon central plate',(0,0,z),(.046,.042,.0018),'Carbon',.002)
    for sx,sy in [(1,-1),(1,1),(-1,1),(-1,-1)]:
        a=math.atan2(sy,sx)
        box('Carbon arm',(sx*.048,sy*.048,z),(.135,.016,.0018),'Carbon',.001,(0,0,a))
for x in [-.018,.018]:
    for y in [-.018,.018]:
        cyl('M3 standoff',(x,y,.011),.0022,.019,'Anodized',24)
        screw((x,y,.0222))
pcb('Four in one ESC',(0,0,.006),(.041,.041))
pcb('Flight controller',(0,0,.020),(.036,.036))
# ESC packages, solder terminals and a perimeter power bus.
for side in [-1,1]:
    for i in range(6):
        x=-.014+i*.0055;y=side*.0135
        box('ESC MOSFET',(x,y,.0082),(.0038,.0048,.0012),'Black',.00015)
        for pin in [-1,1]:box('Solder termination',(x+pin*.0021,y,.00805),(.00055,.0039,.0004),'Steel',.00005)
for i in range(20):
    x=-.014+(i%5)*.007;y=-.012+(i//5)*.008
    box('Soldermask trace',(x,y,.02087),(.0053,.00017,.000045),'Trace',0)
box('MCU package',(0,0,.0221),(.007,.007,.002),'Black',.00025)
box('IMU package',(.010,.004,.0218),(.004,.004,.0012),'Black',.00012)
box('Crystal oscillator',(-.010,.005,.0218),(.004,.0028,.0011),'Steel',.0003)
for i in range(32):
    side=i//8;offset=-.0032+(i%8)*.0009
    x,y=[(offset,-.0039),(.0039,offset),(offset,.0039),(-.0039,offset)][side]
    box('MCU gullwing terminal',(x,y,.0216),(.00038,.00065,.00035),'Steel',.00004)
for i in range(24):
    x=-.013+(i%6)*.0047;y=-.010+(i//6)*.006
    if abs(x)<.005 and abs(y)<.005:continue
    box('SMD component',(x,y,.0215),(.0016,.00085,.00065),'Black',.0001)
    for dx in [-.00078,.00078]:box('SMD end cap',(x+dx,y,.0215),(.00028,.00087,.0006),'Steel',.00003)
for i in range(8):
    x=-.013+i*.0037
    cyl('Gold test pad',(x,-.0157,.02091),.00055,.000055,'Gold',16,0)
text('FC silkscreen','IW  FC',(0,.011,.02096),.0022,'White')
# Low-profile connector housings, contacts and USB socket.
for x in [-.009,.009]:
    box('SH receiver socket',(x,-.018,.022),(.007,.004,.003),'White',.0003)
    for i in range(4):box('SH contact',(x-.0024+i*.0016,-.019,.022),(.00045,.003,.00045),'Gold',.00006)
box('USB housing',(.018,0,.022),(.004,.009,.0035),'Steel',.0006)
box('USB tongue',(.0201,0,.022),(.0002,.0064,.0014),'Black',.0001)
cyl('Electrolytic capacitor',(-.029,0,.013),.0052,.018,'Black',64,.0004,rotation=(0,math.pi/2,0))
cyl('Capacitor top',(-.0382,0,.013),.0046,.0003,'Steel',64,.0001,rotation=(0,math.pi/2,0))
wire('Capacitor lead A',[(-.021,-.002,.012),(-.019,-.003,.007),(-.013,-.004,.007)],.0005,'Red')
wire('Capacitor lead B',[(-.021,.002,.012),(-.019,.003,.007),(-.013,.004,.007)],.0005,'Black')
# Receiver on the inner arm, with cable jacket and antenna coax.
box('Receiver PCB',(.036,.026,.008),(.022,.013,.0015),'PCB',.0006)
box('Receiver protective case',(.036,.026,.011),(.019,.011,.0045),'Black',.0012)
text('Receiver print','RX',(.036,.026,.01335),.003,'Label')
for i in range(4):cyl('Receiver header',(.026,.022+i*.0025,.0087),.00045,.004,'Gold',12,0,rotation=(0,math.pi/2,0))
for i in range(3):wire('Receiver signal wire',[(.026,.022+i*.001,.009),(.021,.022,.011),(.017,.015,.018),(.009,-.018,.022)],.00035,'Red' if i==0 else 'Black')
wire('Antenna coax',[(.042,.028,.011),(.052,.028,.015),(.055,.010,.026),(.048,-.004,.039)],.0005,'Black')
cyl('Antenna sleeve',(.0475,-.004,.045),.001,.014,'Rubber',24,.0003,rotation=(0,-.12,0))
# Four vented outrunner motors, copper stator windings, bearings and arm harnesses.
for index,(sx,sy) in enumerate([(1,-1),(1,1),(-1,1),(-1,-1)]):
    x,y=sx*.095,sy*.095
    cyl('Motor lower mount',(x,y,.0015),.0205,.004,'Anodized',64,.0006)
    cyl('Stator core',(x,y,.010),.0115,.014,'Steel',48,.0001)
    ring('Motor bearing flange',(x,y,.018),.004,.0095,.005,'Anodized',48)
    ring('Motor top rim',(x,y,.021),.0185,.0225,.003,'Anodized',64)
    for tooth in range(12):
        a=tooth*2*math.pi/12
        for loop in range(3):
            bpy.ops.mesh.primitive_torus_add(major_segments=16,minor_segments=6,location=(x+.0145*math.cos(a),y+.0145*math.sin(a),.0065+loop*.0028),major_radius=.0035,minor_radius=.0005)
            finish(bpy.context.object,'Copper stator winding','Copper',smooth=True)
        ring('Motor bell vent rib',(x,y,.012),.021,.0225,.015,'Anodized',5,a-.15,a+.15)
        box('Motor radial spoke',(x+.013*math.cos(a),y+.013*math.sin(a),.020),(.012,.0026,.002),'Anodized',.0004,(0,0,a))
    cyl('Rotor shaft',(x,y,.025),.0022,.008,'Steel',32,.0002)
    for a in [math.pi/4+i*math.pi/2 for i in range(4)]:screw((x+.011*math.cos(a),y+.011*math.sin(a),.0037),.0013)
    for cable in range(3):wire('Motor phase cable',[(x*.25,y*.25+(cable-1)*.0015,.007),(x*.52,y*.52+(cable-1)*.0018,.005),(x*.85,y*.85+(cable-1)*.0018,.003),(x,y,.003)],.0007,'Black')
# Battery tray, shrink wrap, original printed label and woven retaining straps.
box('Battery tray',(-.022,0,-.044),(.103,.054,.0024),'Carbon',.003)
for x in [-.055,.011]:
    for y in [-.022,.022]:cyl('Battery tray post',(x,y,-.024),.0022,.037,'Anodized',24,.00025)
box('LiPo shrink wrap',(-.022,0,-.025),(.092,.043,.029),'Black',.0045)
for y in [-.013,.013]:box('Fabric battery strap',(-.022,y,-.0245),(.095,.006,.032),'Rubber',.0014)
box('Battery printed label',(-.022,-.0217,-.025),(.055,.00025,.019),'Label',.001)
text('Battery label print','IMPERIAL WINGS\n4S  14.8V\n1500mAh',(-.022,-.02195,-.025),.0038,'Ink',(math.pi/2,0,0))
for sign,mat in [(-1,'Red'),(1,'Black')]:wire('Silicone battery lead',[(-.068,sign*.007,-.023),(-.080,sign*.010,-.016),(-.074,sign*.013,.012),(-.049,sign*.009,.014)],.0017,mat)
box('XT60 nylon housing',(-.048,0,.014),(.013,.014,.009),'Yellow',.0018)
for y in [-.0035,.0035]:
    o=ring('XT60 brass terminal',(0,0,0),.0016,.0023,.007,'Gold',32)
    o.rotation_euler=(0,math.pi/2,0);o.location=(-.048,y,.014)
box('Balance connector',(-.074,.019,-.012),(.013,.006,.005),'White',.0007)
for i in range(5):
    y=.014+i*.0023
    wire('Balance lead',[(-.066,y,-.026),(-.078,y,-.024),(-.083,y,-.014),(-.076,.015+i*.0014,-.012)],.00035,'Black' if i==0 else 'Red')
    box('Balance contact',(-.074,.017+i*.0008,-.0094),(.003,.0006,.0003),'Steel',.00005)

def baked_join(name,objects):
    graph=bpy.context.evaluated_depsgraph_get();new=[]
    for obj in list(objects):
        mesh=bpy.data.meshes.new_from_object(obj.evaluated_get(graph),depsgraph=graph)
        fresh=bpy.data.objects.new(obj.name+'_baked',mesh);fresh.matrix_world=obj.matrix_world.copy();bpy.context.collection.objects.link(fresh);new.append(fresh)
    bpy.ops.object.select_all(action='DESELECT')
    for o in new:o.select_set(True)
    bpy.context.view_layer.objects.active=new[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    return o

original=list(bpy.context.scene.objects)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ImperialEquipment.blend'),compress=True)
assembly=baked_join('SM_EquipmentAssembly',original)
for o in original:bpy.data.objects.remove(o,do_unlink=True)

def export(o,name):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.gltf(filepath=str(OUT/(name+'.glb')),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT',export_cameras=False,export_lights=False)

export(assembly,'SM_EquipmentAssembly')
assembly.hide_set(True)
def propeller(name,direction):
    before=set(bpy.context.scene.objects)
    cyl('Prop hub',(0,0,0),.006,.003,'Black',48,.0004)
    ring('Prop washer',(0,0,.002),.0021,.0035,.0007,'Steel',32)
    for blade in range(3):
        angle=blade*2*math.pi/3;verts=[];faces=[];stations=18;section=10
        for i in range(stations):
            t=i/(stations-1);r=.005+t*.047;chord=(.004+.009*math.sin(math.pi*t*.87))*(1-.68*t**4)
            pitch=math.radians(24-14*t)*direction;sweep=.004*t*t*direction
            for j in range(section):
                s=2*math.pi*j/section;u=chord*.5*math.cos(s);z=(.00065*(1-.65*t))*math.sin(s)+.0007*(1-math.cos(s)**2)
                y=sweep+u*math.cos(pitch)-z*math.sin(pitch);zz=u*math.sin(pitch)+z*math.cos(pitch)
                verts.append((r*math.cos(angle)-y*math.sin(angle),r*math.sin(angle)+y*math.cos(angle),zz))
        for i in range(stations-1):
            for j in range(section):faces.append((i*section+j,i*section+(j+1)%section,(i+1)*section+(j+1)%section,(i+1)*section+j))
        faces.extend([tuple(reversed(range(section))),tuple((stations-1)*section+j for j in range(section))])
        mesh=bpy.data.meshes.new('Twisted airfoil blade');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('Twisted airfoil blade',mesh);bpy.context.collection.objects.link(o);finish(o,o.name,'Red',smooth=True)
    parts=[o for o in bpy.context.scene.objects if o not in before]
    result=baked_join(name,parts)
    for o in parts:bpy.data.objects.remove(o,do_unlink=True)
    export(result,name);result.hide_set(True)
    return len(result.data.polygons)
prop_polys=propeller('SM_PropellerCW',-1)+propeller('SM_PropellerCCW',1)
corners=[assembly.matrix_world@Vector(c) for c in assembly.bound_box]
manifest=dict(design='Original reference-informed equipment; no manufacturer meshes, photos or logos',units='metres',motor_xy_m=.095,
              assumed_battery_profile=dict(cells=4,volts=14.8,capacity_mah=1500),assembly_polygons=len(assembly.data.polygons),propeller_polygons=prop_polys,
              source_objects=len(original),bounds_min=[min(v[i] for v in corners) for i in range(3)],bounds_max=[max(v[i] for v in corners) for i in range(3)],materials=len(assembly.data.materials))
(OUT/'electronics-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('IMPERIAL_EQUIPMENT '+json.dumps(manifest))
