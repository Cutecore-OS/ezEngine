import base64, copy, json, pathlib, struct
root = pathlib.Path(__file__).resolve().parent / 'Data'
blob = bytearray()
views, accessors = [], []
def acc(values, kind='SCALAR', components=1):
    while len(blob) % 4: blob.append(0)
    start = len(blob)
    blob.extend(struct.pack('<' + 'f'*len(values), *values))
    views.append(dict(buffer=0, byteOffset=start, byteLength=len(blob)-start))
    d = dict(bufferView=len(views)-1, componentType=5126, count=len(values)//components, type=kind)
    if kind == 'VEC3':
        d['min'] = [min(values[c::3]) for c in range(3)]
        d['max'] = [max(values[c::3]) for c in range(3)]
    accessors.append(d)
    return len(accessors)-1
p = acc([0,0,0,1,0,0,0,1,0], 'VEC3', 3)
n = acc([0,0,1]*3, 'VEC3', 3)
uv = acc([0,0,1,0,0,1], 'VEC2', 2)
s = acc([0,0,1,0,0,0,0,0,0], 'VEC3', 3)
b = acc([0,0,0,0,0,2,0,0,0], 'VEC3', 3)
times = acc([0,1])
linear = acc([0,0,1,-1])
cubic = acc([0,0, 0,0, 2,-2, 0,0, 1,-1, 0,0])
animations = []
for name, interpolation, values in [('Linear','LINEAR',linear),('Step','STEP',linear),('Cubic','CUBICSPLINE',cubic)]:
    animations.append(dict(name=name, samplers=[dict(input=times,output=values,interpolation=interpolation)], channels=[dict(sampler=0,target=dict(node=0,path='weights'))]))
doc = dict(asset=dict(version='2.0'), scene=0, scenes=[dict(nodes=[0])], nodes=[dict(name='Face', mesh=0)], meshes=[dict(name='FaceMesh', weights=[0,0], extras=dict(targetNames=['Smile','Blink']), primitives=[dict(attributes=dict(POSITION=p,NORMAL=n,TEXCOORD_0=uv),targets=[dict(POSITION=s),dict(POSITION=b)])])], animations=animations, bufferViews=views, accessors=accessors, buffers=[dict(byteLength=len(blob))])
json_bytes = json.dumps(doc, separators=(',', ':')).encode()
json_bytes += b' ' * (-len(json_bytes)%4)
bin_bytes = bytes(blob) + b'\0' * (-len(blob)%4)
glb = struct.pack('<III',0x46546c67,2,12+8+len(json_bytes)+8+len(bin_bytes))
glb += struct.pack('<II',len(json_bytes),0x4e4f534a)+json_bytes
glb += struct.pack('<II',len(bin_bytes),0x004e4942)+bin_bytes
(root/'Morph.glb').write_bytes(glb)
doc['buffers'][0]['uri'] = 'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()
(root/'Morph.gltf').write_text(json.dumps(doc, indent=2))
invalid = copy.deepcopy(doc)
invalid['accessors'][times]['count'] = 9000000
(root/'Invalid.gltf').write_text(json.dumps(invalid, indent=2))

# Additional semantic fixtures: node defaults override mesh defaults; normal deltas stay linear.
node_default = copy.deepcopy(doc)
node_default['nodes'][0]['weights'] = [0.3,-0.2]
(root/'NodeDefaults.gltf').write_text(json.dumps(node_default, indent=2))
# Replace the first morph position accessor by a sparse representation of the same data.
sparse = copy.deepcopy(doc)
# The normal fixture reuses the position delta as a normal delta (at vertex 0, +Z).
normal = copy.deepcopy(doc)
normal['meshes'][0]['primitives'][0]['targets'][0]['NORMAL'] = s
(root/'Normal.gltf').write_text(json.dumps(normal, indent=2))

# One joint with animated translation, combined with the same morph animation.
skinned = copy.deepcopy(doc)
joint_weights = acc([1,0,0,0]*3, 'VEC4', 4)
bind_pose = acc([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], 'MAT4', 16)
translation = acc([0,0,0, 0,2,0], 'VEC3', 3)
start = len(blob)
blob.extend(struct.pack('<12H', *([0]*12)))
views.append(dict(buffer=0, byteOffset=start, byteLength=24))
accessors.append(dict(bufferView=len(views)-1, componentType=5123, count=3, type='VEC4'))
skinned['bufferViews'] = copy.deepcopy(views)
skinned['accessors'] = copy.deepcopy(accessors)
skinned['buffers'] = [dict(byteLength=len(blob),uri='data:application/octet-stream;base64,'+base64.b64encode(blob).decode())]
skinned['meshes'][0]['primitives'][0]['attributes'].update(JOINTS_0=len(accessors)-1, WEIGHTS_0=joint_weights)
skinned['nodes'][0]['skin'] = 0
skinned['nodes'].append(dict(name='Root'))
skinned['scenes'][0]['nodes'].append(1)
skinned['skins'] = [dict(joints=[1],skeleton=1,inverseBindMatrices=bind_pose)]
skinned['animations'] = [copy.deepcopy(animations[0])]
skinned['animations'][0]['samplers'].append(dict(input=times,output=translation,interpolation='LINEAR'))
skinned['animations'][0]['channels'].append(dict(sampler=1,target=dict(node=1,path='translation')))
(root/'Skinned.gltf').write_text(json.dumps(skinned, indent=2))
skinned['animations'][0]['samplers'][1]['interpolation'] = 'STEP'
(root/'SkeletalStep.gltf').write_text(json.dumps(skinned, indent=2))

# A separate animation file without morph geometry or weight channels.
skeleton_only = copy.deepcopy(skinned)
skeleton_only['animations'][0]['samplers'][1]['interpolation'] = 'LINEAR'
skeleton_only['animations'][0]['channels'] = [c for c in skeleton_only['animations'][0]['channels'] if c['target']['path'] != 'weights']
for mesh in skeleton_only['meshes']:
    mesh.pop('weights', None)
    mesh.pop('extras', None)
    for primitive in mesh['primitives']:
        primitive.pop('targets', None)
(root/'SkeletonOnly.gltf').write_text(json.dumps(skeleton_only, indent=2))
