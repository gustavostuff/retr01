-- World layout generation. Capped at a 16x16 grid footprint.

local Worlds = {}

local maxScreens = 64
local startX, startY = 128, 128

local function inBounds(x, y)
  -- Generate only within a 16x16 virtual-grid footprint.
  -- (The bounds clamp below is what guarantees the final grid size.)
  return math.abs(x - startX) <= 8 and math.abs(y - startY) <= 8
end

local function computeBounds(world)
  if #world.cells == 0 then
    return
  end

  world.bounds.minX, world.bounds.maxX = world.cells[1].x, world.cells[1].x
  world.bounds.minY, world.bounds.maxY = world.cells[1].y, world.cells[1].y
  for _, cell in ipairs(world.cells) do
    if cell.x < world.bounds.minX then world.bounds.minX = cell.x end
    if cell.x > world.bounds.maxX then world.bounds.maxX = cell.x end
    if cell.y < world.bounds.minY then world.bounds.minY = cell.y end
    if cell.y > world.bounds.maxY then world.bounds.maxY = cell.y end
  end

  if (world.bounds.maxX - world.bounds.minX + 1) > 16 then
    world.bounds.maxX = world.bounds.minX + 15
  end
  if (world.bounds.maxY - world.bounds.minY + 1) > 16 then
    world.bounds.maxY = world.bounds.minY + 15
  end
end

local function generateSingle(world)
  table.insert(world.cells, {x = startX, y = startY})
end

local function generateLinearHoriz(world)
  local length = love.math.random(8, 16)
  for i = 0, length - 1 do
    table.insert(world.cells, {x = startX - math.floor(length / 2) + i, y = startY})
  end
end

local function generateLinearVert(world)
  local length = love.math.random(8, 16)
  for i = 0, length - 1 do
    table.insert(world.cells, {x = startX, y = startY - math.floor(length / 2) + i})
  end
end

local function generateSnake(world)
  local visited = {}
  local cx, cy = startX, startY
  table.insert(world.cells, {x = cx, y = cy})
  visited[cx .. "," .. cy] = true

  local dirs = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
  local currentDir = love.math.random(1, 4)
  local steps = 0
  local maxSteps = love.math.random(4, 10)

  while #world.cells < maxScreens do
    local dx, dy = dirs[currentDir][1], dirs[currentDir][2]
    local nx, ny = cx + dx, cy + dy

    if inBounds(nx, ny) then
      if not visited[nx .. "," .. ny] then
        cx, cy = nx, ny
        table.insert(world.cells, {x = cx, y = cy})
        visited[cx .. "," .. cy] = true
        steps = steps + 1

        if steps >= maxSteps then
          currentDir = (currentDir <= 2) and love.math.random(3, 4) or love.math.random(1, 2)
          steps = 0
          maxSteps = love.math.random(4, 10)
        end
      else
        local turnOpts = (currentDir <= 2) and {3, 4} or {1, 2}
        local freeOpts = {}
        for _, opt in ipairs(turnOpts) do
          local tx, ty = cx + dirs[opt][1], cy + dirs[opt][2]
          if inBounds(tx, ty) and not visited[tx .. "," .. ty] then
            table.insert(freeOpts, opt)
          end
        end

        if #freeOpts > 0 then
          currentDir = freeOpts[love.math.random(#freeOpts)]
          steps = 0
          maxSteps = love.math.random(4, 10)
        else
          break
        end
      end
    else
      currentDir = love.math.random(1, 4)
      steps = 0
    end
  end
end

local function generatePackedGrid(world)
  for y = 0, 7 do
    for x = 0, 7 do
      table.insert(world.cells, {x = startX - 4 + x, y = startY - 4 + y})
    end
  end
end

local function generateHoleGrid(world)
  for y = 0, 7 do
    for x = 0, 7 do
      if love.math.random() > love.math.random(30, 50) / 100 then
        table.insert(world.cells, {x = startX - 4 + x, y = startY - 4 + y})
      end
    end
  end
end

local function generateRandomCluster(world)
  local visited = {}
  table.insert(world.cells, {x = startX, y = startY})
  visited[startX .. "," .. startY] = true

  while #world.cells < maxScreens do
    local base = world.cells[love.math.random(#world.cells)]
    local dx, dy = 0, 0
    local r = love.math.random(1, 4)
    if r == 1 then dx = 1 elseif r == 2 then dx = -1 elseif r == 3 then dy = 1 else dy = -1 end

    local nx, ny = base.x + dx, base.y + dy
    if inBounds(nx, ny) and not visited[nx .. "," .. ny] then
      visited[nx .. "," .. ny] = true
      table.insert(world.cells, {x = nx, y = ny})
    end
  end
end

local layouts = {
  {name = "1x1 Single", generate = generateSingle},
  {name = "Linear Horiz", generate = generateLinearHoriz},
  {name = "Linear Vert", generate = generateLinearVert},
  {name = "Snake Path", generate = generateSnake},
  {name = "Packed Grid", generate = generatePackedGrid},
  {name = "Hole Grid", generate = generateHoleGrid},
  {name = "Random Cluster", generate = generateRandomCluster},
}

function Worlds.build(layout)
  local world = {name = layout.name, cells = {}, bounds = {}}
  layout.generate(world)
  computeBounds(world)
  return world
end

function Worlds.generateAll()
  local worlds = {}
  for _, layout in ipairs(layouts) do
    table.insert(worlds, Worlds.build(layout))
  end
  return worlds
end

return Worlds
