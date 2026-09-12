import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False, 'assets': []}
try:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    for folder in ['/Game/ParagonGreystone/Characters', '/Game/ParagonMinions/Characters']:
        for asset in registry.get_assets_by_path(folder, recursive=True):
            kind = str(asset.asset_class_path.asset_name)
            if kind not in ['SkeletalMesh', 'Skeleton', 'AnimBlueprint', 'AnimSequence', 'BlendSpace', 'BlendSpace1D', 'AnimMontage']:
                continue
            entry = {'path': str(asset.package_name), 'kind': kind}
            for tag in ['Skeleton', 'TargetSkeleton', 'ParentClass']:
                value = asset.get_tag_value(tag)
                if value:
                    entry[tag] = str(value)
            result['assets'].append(entry)
    result['passed'] = len(result['assets']) > 0
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/character-inventory.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result.get('error', 'No character assets found'))
