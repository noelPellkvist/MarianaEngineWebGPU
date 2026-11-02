-- The types of the Mariana script API for Luau's type checker: the engine's own (MarianaScriptTools:
-- the editor's script errors and autocomplete) and luau-lsp's in VS Code. The engine puts a type per
-- component the app registers where the "@components" and "@componentMethods" lines are; the whole
-- file is written to a project's .mariana/types/Mariana.d.luau.

declare function vec3(x: number?, y: number?, z: number?): vector
declare function warn(...: any): ()

declare extern type quat with
    x: number
    y: number
    z: number
    w: number
    function Inverse(self): quat
    function ToEuler(self): vector
    function __mul(self, other: quat): quat
    function __mul(self, other: vector): vector
end

declare quat: {
    identity: quat,
    new: (x: number, y: number, z: number, w: number) -> quat,
    euler: ((x: number, y: number, z: number) -> quat) & ((angles: vector) -> quat),
    angleAxis: (degrees: number, axis: vector) -> quat,
    lookRotation: (forward: vector, up: vector?) -> quat,
    fromToRotation: (from: vector, to: vector) -> quat,
    slerp: (a: quat, b: quat, t: number) -> quat,
    angle: (a: quat, b: quat) -> number,
}

-- A component as a script sees it: its fields, as scene files save them (and a few that only code
-- sets, like a vehicle's throttle), and for some methods.
declare extern type Component with
end

declare extern type AnimatorMethods extends Component with
    function SetFloat(self, parameter: string, value: number): ()
    function SetBool(self, parameter: string, value: boolean): ()
    function SetTrigger(self, parameter: string): ()
    function ResetTrigger(self, parameter: string): ()
    function GetFloat(self, parameter: string): number
    function GetBool(self, parameter: string): boolean
    -- Vector parameters: world positions (IK targets).
    function SetVector(self, parameter: string, value: vector): ()
    function GetVector(self, parameter: string): vector
    function Play(self, clip: string | number, loop: boolean?): ()
    function CrossFade(self, clip: string | number, seconds: number?, loop: boolean?): ()
    function CrossFadeState(self, state: string, seconds: number?): boolean
    -- Root motion of the last update: the walk (m/s, the model's own axes) and the turn (deg/s).
    function GetRootVelocity(self): (vector, number)
end

-- @components

-- An image file (texture.load), read in the colour space of the material slot it is put in.
declare extern type Texture with
    path: string
end

-- How a surface looks. Materials scripts get are their own: material.new and Clone make new ones,
-- and entity:GetMaterial gives the entity a copy the first time.
declare extern type Material with
    name: string
    color: vector
    alpha: number
    metallic: number
    roughness: number
    emissive: vector
    normalStrength: number
    occlusionStrength: number
    clearcoat: number
    clearcoatRoughness: number
    doubleSided: boolean
    alphaMode: "Opaque" | "Mask" | "Blend"
    alphaCutoff: number
    tiling: vector
    offset: vector
    baseColorMap: Texture?
    normalMap: Texture?
    metallicRoughnessMap: Texture?
    occlusionMap: Texture?
    emissiveMap: Texture?
    function Clone(self): Material
end

export type MaterialFields = {
    color: vector?,
    alpha: number?,
    metallic: number?,
    roughness: number?,
    emissive: vector?,
    normalStrength: number?,
    occlusionStrength: number?,
    clearcoat: number?,
    clearcoatRoughness: number?,
    doubleSided: boolean?,
    alphaMode: ("Opaque" | "Mask" | "Blend")?,
    alphaCutoff: number?,
    tiling: vector?,
    offset: vector?,
    baseColorMap: Texture?,
    normalMap: Texture?,
    metallicRoughnessMap: Texture?,
    occlusionMap: Texture?,
    emissiveMap: Texture?,
}

-- What mesh.new makes a mesh from: per vertex a position (and optionally a normal, uv and colour),
-- as tables of vectors or buffers of 32-bit floats; triangles (or lines, points) by indices from 1
-- in tables, from 0 in buffers of 32-bit integers.
export type MeshData = {
    positions: { vector } | buffer,
    normals: ({ vector } | buffer)?,
    uvs: ({ vector } | buffer)?,
    colors: ({ vector } | buffer)?,
    indices: ({ number } | buffer)?,
    submeshes: { { number } | buffer }?,
    topology: ("triangles" | "lines" | "points")?,
    smooth: boolean?,
    dynamic: boolean?,
}
export type MeshUpdate = {
    positions: ({ vector } | buffer)?,
    normals: ({ vector } | buffer)?,
    uvs: ({ vector } | buffer)?,
    colors: ({ vector } | buffer)?,
}

declare extern type Mesh with
    vertexCount: number
    boundsMin: vector
    boundsMax: vector
    dynamic: boolean
    function Update(self, data: MeshUpdate): ()
end

declare extern type Entity with
    id: number
    name: string
    parent: Entity?
    position: vector
    rotation: quat
    scale: vector
    worldPosition: vector
    worldRotation: quat
    forward: vector
    right: vector
    up: vector
    function IsValid(self): boolean
    function Destroy(self): ()
    function Has(self, component: ComponentName): boolean
    function Remove(self, component: ComponentName): ()
    function GetScript(self, name: string): any
    function GetChildren(self): { Entity }
    function FindChild(self, name: string): Entity?
    function Rotate(self, axis: vector, degrees: number): ()
    function Translate(self, offset: vector): ()
    function LookAt(self, target: vector | Entity, up: vector?): ()
    function SetMesh(self, mesh: Mesh, materials: { Material }?): ()
    function GetMesh(self): Mesh?
    function GetMaterial(self, slot: number?): Material?
    function SetMaterial(self, material: Material, slot: number?): ()
    -- @componentMethods
end

-- What a script's table and its `self` are (`local Spin = {} :: Script`): `entity` typed, the rest
-- (properties, the script's own state and functions) free.
export type Script = { entity: Entity, properties: { [string]: any }?, [string]: any }

export type Contact = { point: vector, normal: vector, speed: number }
export type RaycastHit = { entity: Entity, point: vector, normal: vector, distance: number }
export type QueryOptions = { ignore: Entity?, triggers: boolean?, layers: number? }

declare mesh: {
    new: (data: MeshData) -> Mesh,
}

declare material: {
    new: (fields: MaterialFields?) -> Material,
}

declare texture: {
    load: (path: string) -> Texture,
}

declare scene: {
    Find: (name: string) -> Entity?,
    Create: (name: string?, parent: Entity?) -> Entity,
    Spawn: (path: string, position: vector?, rotation: quat?) -> Entity,
    SpawnPrimitive: (shape: "cube" | "sphere" | "plane" | "cylinder" | "capsule", position: vector?, color: vector?, rotation: quat?) -> Entity,
    Each: (component: ComponentName, fn: (Entity) -> ()) -> (),
    Destroy: (entity: Entity) -> (),
}

declare input: {
    GetKey: (name: string) -> boolean,
    GetKeyDown: (name: string) -> boolean,
    GetKeyUp: (name: string) -> boolean,
    GetMouseButton: (button: number?) -> boolean,
    GetMouseButtonDown: (button: number?) -> boolean,
    GetMouseButtonUp: (button: number?) -> boolean,
    GetMousePosition: () -> vector,
    GetMouseDelta: () -> vector,
    GetScroll: () -> vector,
}

declare extern type Time with
    delta: number
    time: number
    frame: number
    scale: number
end

declare time: Time

export type BodyInfo = {
    motion: "Static" | "Kinematic" | "Dynamic",
    sleeping: boolean,
    sensor: boolean,
    mass: number,
    linearVelocity: vector,
    angularVelocity: vector,
}
export type WheelInfo = {
    contact: boolean,
    suspension: number,
    steerAngle: number,
    rpm: number,
    longitudinalSlip: number,
    lateralSlip: number,
}
export type VehicleInfo = { speed: number, rpm: number, gear: number, wheels: { WheelInfo } }
export type CharacterInfo = {
    grounded: boolean,
    onSteepGround: boolean,
    velocity: vector,
    groundNormal: vector,
    ground: Entity?,
}

declare physics: {
    Raycast: (origin: vector, direction: vector, maxDistance: number?, options: QueryOptions?) -> RaycastHit?,
    RaycastAll: (origin: vector, direction: vector, maxDistance: number?, options: QueryOptions?) -> { RaycastHit },
    SphereCast: (origin: vector, radius: number, direction: vector, maxDistance: number?, options: QueryOptions?) -> RaycastHit?,
    OverlapSphere: (center: vector, radius: number, options: QueryOptions?) -> { Entity },
    OverlapBox: (center: vector, halfExtents: vector, rotation: quat?, options: QueryOptions?) -> { Entity },
    AddForce: (entity: Entity, force: vector) -> boolean,
    AddForceAtPoint: (entity: Entity, force: vector, point: vector) -> boolean,
    AddTorque: (entity: Entity, torque: vector) -> boolean,
    AddImpulse: (entity: Entity, impulse: vector) -> boolean,
    AddImpulseAtPoint: (entity: Entity, impulse: vector, point: vector) -> boolean,
    AddAngularImpulse: (entity: Entity, impulse: vector) -> boolean,
    AddRagdollImpulse: (entity: Entity, impulse: vector) -> boolean,
    SetVelocity: (entity: Entity, linear: vector, angular: vector?) -> boolean,
    GetVelocity: (entity: Entity) -> (vector?, vector?),
    Teleport: (entity: Entity, position: vector, rotation: quat?) -> boolean,
    WakeUp: (entity: Entity) -> boolean,
    IsTouching: (a: Entity, b: Entity) -> boolean,
    GetBodyInfo: (entity: Entity) -> BodyInfo?,
    GetVehicleInfo: (entity: Entity) -> VehicleInfo?,
    GetCharacterInfo: (entity: Entity) -> CharacterInfo?,
    SetRagdollMode: (entity: Entity, mode: "Animated" | "Powered" | "Limp") -> (),
    WaterAt: (point: vector) -> (vector?, vector?),
}
