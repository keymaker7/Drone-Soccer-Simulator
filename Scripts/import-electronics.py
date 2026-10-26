"""Import original Blender equipment into authored, cooked Unreal static meshes."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
tools=unreal.AssetToolsHelpers.get_asset_tools()
reports=[]
for name in ['SM_EquipmentAssembly','SM_PropellerCW','SM_PropellerCCW']:
    task=unreal.AssetImportTask()
    task.filename=str(root/'ArtSource'/'Airframe'/(name+'.glb'))
    task.destination_path='/Game/Airframe'
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    tools.import_asset_tasks([task])
    meshes=[o for o in task.get_objects() if isinstance(o,unreal.StaticMesh)]
    if len(meshes)!=1:raise RuntimeError('Expected exactly one static mesh: '+name+' '+str(task.imported_object_paths))
    mesh=meshes[0]
    expected='/Game/Airframe/'+name
    if mesh.get_path_name().split('.')[0]!=expected:
        if not unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),expected):raise RuntimeError('Cannot assign stable mesh path')
    bounds=mesh.get_bounding_box()
    minimum=bounds.min;maximum=bounds.max
    extent=maximum-minimum
    if name=='SM_EquipmentAssembly' and not 20<extent.x<26:raise RuntimeError('Unexpected GLTF metre-to-centimetre conversion: '+str(extent))
    if name.startswith('SM_Propeller') and not 8<extent.x<12:raise RuntimeError('Unexpected propeller dimensions: '+str(extent))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    reports.append(dict(name=name,path=mesh.get_path_name(),dimensions_cm=[extent.x,extent.y,extent.z],materials=len(mesh.static_materials)))
unreal.EditorAssetLibrary.save_directory('/Game/Airframe',only_if_is_dirty=False,recursive=True)
path=root/'Saved'/'equipment-import.json'
path.write_text(json.dumps(dict(ok=True,assets=reports),indent=2),encoding='utf-8')
unreal.log('IMPERIAL_EQUIPMENT_IMPORT '+json.dumps(reports))
