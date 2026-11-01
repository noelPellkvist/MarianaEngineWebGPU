--!strict
-- Snake, part 1: the board and a snake that stands still.
local Snake = {} :: Script

Snake.properties = {
    size = 15,          -- the board is size x size cells of 1 m
    stepTime = 0.16,    -- seconds per step at the start
    speedUp = 0.96,     -- each apple makes a step take this much of the time before
    startLength = 3,    -- segments at the start
}

local HEAD_COLOR = vec3(0.35, 0.95, 0.35)
local BODY_COLOR = vec3(0.15, 0.6, 0.2)
local WALL_COLOR = vec3(0.25, 0.27, 0.32)
local FLOOR_COLOR = vec3(0.75, 0.78, 0.72)

-- Where a cell (whole numbers x and z) is in the world: the board is centred on the entity.
local function cellToWorld(self: Script, cell: vector): vector
    local half = (self.size - 1) / 2
    return self.entity.worldPosition + vec3(cell.x - half, 0.45, cell.z - half)
end

-- The floor and the four walls, made once.
local function buildBoard(self: Script)
    local center = self.entity.worldPosition
    local floor = scene.SpawnPrimitive("plane", center, FLOOR_COLOR)
    floor.name = "Floor"
    floor.scale = vec3(self.size / 10, 1, self.size / 10) -- the plane is 10 m across

    local half = self.size / 2 + 0.5
    local walls = {
        { vec3(0, 0.5, half), vec3(self.size + 2, 1, 1) },
        { vec3(0, 0.5, -half), vec3(self.size + 2, 1, 1) },
        { vec3(half, 0.5, 0), vec3(1, 1, self.size) },
        { vec3(-half, 0.5, 0), vec3(1, 1, self.size) },
    }
    for _, wall in walls do
        local block = scene.SpawnPrimitive("cube", center + wall[1], WALL_COLOR)
        block.name = "Wall"
        block.scale = wall[2]
    end
end

local function addSegment(self: Script, cell: vector, color: vector): Entity
    local segment = scene.SpawnPrimitive("cube", cellToWorld(self, cell), color)
    segment.name = "Segment"
    segment.scale = vec3(0.85, 0.85, 0.85)
    table.insert(self.segments, segment)
    return segment
end

-- A new game: the snake in the middle of the board, its head on the right.
local function newGame(self: Script)
    self.cells = {}
    self.segments = {}
    local middle = math.floor(self.size / 2)
    for i = 0, self.startLength - 1 do
        local cell = vec3(middle - i, 0, middle)
        table.insert(self.cells, cell)
        addSegment(self, cell, if i == 0 then HEAD_COLOR else BODY_COLOR)
    end
end

function Snake.Start(self: Script)
    buildBoard(self)
    newGame(self)
end

return Snake
