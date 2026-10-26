import unreal
import math
import os
import random
import struct
import zlib

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def material(name, translucent=False):
    path = '/Game/Materials/' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mat:
        unreal.MaterialEditingLibrary.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        return mat
    mat = asset_tools.create_asset(name, '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    lib.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -100)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.2, 0.7, 0.6, 1))
    glow = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 130)
    glow.set_editor_property('parameter_name', 'Glow')
    glow.set_editor_property('default_value', 0.0)
    mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 100)
    lib.connect_material_expressions(color, '', mul, 'A')
    lib.connect_material_expressions(glow, '', mul, 'B')
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -220, 260)
    rough.set_editor_property('r', 0.72)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if translucent:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('two_sided', True)
        opacity = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -220, 400)
        opacity.set_editor_property('parameter_name', 'Opacity')
        opacity.set_editor_property('default_value', 0.12)
        lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

material('M_Surface')
material('M_Zone', True)

# Reproducible microstructure textures, generated from source rather than fetched assets.
# They keep every material inspectable and avoid dependencies on external art packs.
def png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack('!I', len(data)) + tag + data + struct.pack('!I', zlib.crc32(tag + data) & 0xffffffff)
    scan = b''.join(b'\0' + bytes(rgb[y*width*3:(y+1)*width*3]) for y in range(height))
    with open(path, 'wb') as out:
        out.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB',width,height,8,2,0,0,0)) + chunk(b'IDAT',zlib.compress(scan,9)) + chunk(b'IEND',b''))

source = os.path.join(unreal.Paths.project_dir(), 'ArtSource', 'Materials')
os.makedirs(source, exist_ok=True)
resolution = 256
rng = random.Random(82143)
height = [rng.random() for _ in range(resolution*resolution)]
grain, concrete, normal, carbon = [], [], [], []
for y in range(resolution):
    for x in range(resolution):
        h = height[y*resolution+x]
        coarse = (math.sin(x*.11)*math.sin(y*.07)+math.sin((x+y)*.037))*.08
        a = int(max(0,min(255,214+h*35)))
        c = int(max(0,min(255,180+(h-.5)*35+coarse*90)))
        dx = height[y*resolution+(x+1)%resolution]-height[y*resolution+(x-1)%resolution]
        dy = height[((y+1)%resolution)*resolution+x]-height[((y-1)%resolution)*resolution+x]
        grain.extend((a,a,a)); concrete.extend((c,c,c))
        normal.extend((int(128+dx*12),int(128+dy*12),254))
        weave=(x//8+y//8)%2;thread=(x if weave else y)%8;shade=int(155+55*math.sin((thread+.5)*math.pi/8)+h*15);carbon.extend((shade,shade,shade))

textures = {}
for name, data, is_normal in [('T_SurfaceGrain',grain,False),('T_ConcreteGrain',concrete,False),('T_SurfaceNormal',normal,True),('T_CarbonWeave',carbon,False)]:
    file = os.path.join(source,name+'.png')
    png(file,resolution,resolution,data)
    task=unreal.AssetImportTask();task.filename=file;task.destination_path='/Game/Textures';task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True
    asset_tools.import_asset_tasks([task])
    tex=unreal.load_asset('/Game/Textures/'+name)
    tex.set_editor_property('srgb',False)
    if is_normal: tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    textures[name]=tex

def pbr(name, roughness, metallic=0.0, concrete=False, carbon=False):
    path='/Game/Materials/'+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mat:
        return mat
    mat=asset_tools.create_asset(name,'/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    lib.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    def node(kind,x,y): return lib.create_material_expression(mat,kind,x,y)
    color=node(unreal.MaterialExpressionVectorParameter,-650,-180);color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(.22,.24,.25,1))
    position=node(unreal.MaterialExpressionWorldPosition,-1000,0)
    mask=node(unreal.MaterialExpressionComponentMask,-820,0)
    for key,val in [('r',True),('g',True),('b',False),('a',False)]:mask.set_editor_property(key,val)
    lib.connect_material_expressions(position,'',mask,'Input')
    scale=node(unreal.MaterialExpressionMultiply,-650,0);scale.set_editor_property('const_b',.01)
    lib.connect_material_expressions(mask,'',scale,'A')
    sample=node(unreal.MaterialExpressionTextureSample,-450,0);sample.set_editor_property('texture',textures['T_CarbonWeave' if carbon else ('T_ConcreteGrain' if concrete else 'T_SurfaceGrain')])
    sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    lib.connect_material_expressions(scale,'',sample,'Coordinates')
    base=node(unreal.MaterialExpressionMultiply,-190,-150)
    lib.connect_material_expressions(color,'',base,'A');lib.connect_material_expressions(sample,'RGB',base,'B')
    lib.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(unreal.MaterialExpressionScalarParameter,-450,260);rough.set_editor_property('parameter_name','Roughness');rough.set_editor_property('default_value',roughness)
    lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    metal=node(unreal.MaterialExpressionScalarParameter,-450,360);metal.set_editor_property('parameter_name','Metallic');metal.set_editor_property('default_value',metallic)
    lib.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
    nm=node(unreal.MaterialExpressionTextureSample,-450,460);nm.set_editor_property('texture',textures['T_SurfaceNormal']);nm.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    lib.connect_material_expressions(scale,'',nm,'Coordinates');lib.connect_material_property(nm,'RGB',unreal.MaterialProperty.MP_NORMAL)
    glow=node(unreal.MaterialExpressionScalarParameter,-450,620);glow.set_editor_property('parameter_name','Glow');glow.set_editor_property('default_value',0)
    em=node(unreal.MaterialExpressionMultiply,-190,620);lib.connect_material_expressions(color,'',em,'A');lib.connect_material_expressions(glow,'',em,'B')
    lib.connect_material_property(em,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

pbr('M_Court',.48)
pbr('M_Concrete',.84,concrete=True)
pbr('M_Metal',.32,.85)
pbr('M_Rubber',.82)
pbr('M_Carbon',.38,.05,carbon=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/L_DroneSoccerArena'):
    if not level.new_level('/Game/Maps/L_DroneSoccerArena'):
        raise RuntimeError('Unable to create L_DroneSoccerArena map')
    if not level.save_current_level():
        raise RuntimeError('Unable to save L_DroneSoccerArena map')
unreal.log('IMPERIAL_CONTENT_READY')
