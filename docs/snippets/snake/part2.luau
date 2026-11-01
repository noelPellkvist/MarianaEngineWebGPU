--!strict
-- Snake, part 2: the snake moves one cell at a time and the arrow keys steer it.
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

-- The arrow keys and W A S D as directions on the board (+Z is "up" on the screen).
local KEYS: { [string]: vector } = {
    up = vec3(0, 0, 1), w = vec3(0, 0, 1),
    down = vec3(0, 0, -1), s = vec3(0, 0, -1),
    left = vec3(-1, 0, 0), a = vec3(-1, 0, 0),
    right = vec3(1, 0, 0), d = vec3(1, 0, 0),
}

-- Where a cell (whole numbers x and z) is in the world: the board is centred on the entity.
local function cellToWorld(self: Script, cell: vector): vector
    local half = (self.size - 1) / 2
    return self.entity.worldPosition + vec3(cell.x - half, 0.45, cell.z - half)
end

local function sameCell(a: vector, b: vector): boolean
    return a.x == b.x and a.z == b.z
end

local function isInside(self: Script, cell: vector): boolean
    return cell.x >= 0 and cell.z >= 0 and cell.x < self.size and cell.z < self.size
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

-- A new game: the snake in the middle of the board, heading right.
local function newGame(self: Script)
    self.cells = {}
    self.segments = {}
    local middle = math.floor(self.size / 2)
    for i = 0, self.startLength - 1 do
        local cell = vec3(middle - i, 0, middle)
        table.insert(self.cells, cell)
        addSegment(self, cell, if i == 0 then HEAD_COLOR else BODY_COLOR)
    end
    self.direction = vec3(1, 0, 0)
    self.queued = {}
    self.timer = 0
    self.currentStep = self.stepTime
    self.over = false
end

-- One step: the head moves on a cell and every other segment takes the cell in front of it.
local function step(self: Script)
    if #self.queued > 0 then
        self.direction = table.remove(self.queued, 1) :: vector
    end
    local head = self.cells[1] + self.direction

    -- Walls and its own body (the tail moves away this step, so it doesn't count).
    if not isInside(self, head) then
        self.over = true
        print("Game over: you hit the wall")
        return
    end
    for i = 1, #self.cells - 1 do
        if sameCell(self.cells[i], head) then
            self.over = true
            print("Game over: you bit yourself")
            return
        end
    end

    table.insert(self.cells, 1, head)
    table.remove(self.cells)
    for i, segment in self.segments do
        segment.position = cellToWorld(self, self.cells[i])
    end
    self.segments[1].rotation = quat.lookRotation(self.direction)
end

function Snake.Start(self: Script)
    buildBoard(self)
    newGame(self)
end

function Snake.Update(self: Script, dt: number)
    if self.over then
        return
    end

    -- Turns are queued (two at most), so quick key presses all count; reversing isn't allowed.
    for key, direction in KEYS do
        if input.GetKeyDown(key) and #self.queued < 2 then
            local heading = if #self.queued > 0 then self.queued[#self.queued] else self.direction
            if not sameCell(direction, heading) and not sameCell(direction, -heading) then
                table.insert(self.queued, direction)
            end
        end
    end

    self.timer += dt
    while self.timer >= self.currentStep and not self.over do
        self.timer -= self.currentStep
        step(self)
    end
end

return Snake
