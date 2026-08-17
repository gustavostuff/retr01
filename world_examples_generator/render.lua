-- Draws the 4x2 world card grid onto the internal 640x360 canvas.

local Render = {}

local CANVAS_W, CANVAS_H = 640, 360
local COLS, ROWS = 4, 2
local WINDOW_SCALE = 2

local function drawWorldViewport(world, vx, vy, vw, vh)
  love.graphics.setColor(0.3, 0.3, 0.3)
  love.graphics.rectangle("line", vx + 2, vy + 2, vw - 4, vh - 4)

  if #world.cells == 0 then return end

  local gridW = world.bounds.maxX - world.bounds.minX + 1
  local gridH = world.bounds.maxY - world.bounds.minY + 1

  love.graphics.setColor(0.8, 0.8, 0.8)
  love.graphics.print(world.name, vx + 8, vy + 6)
  love.graphics.setColor(0.5, 0.8, 0.5)
  love.graphics.print("Grid: " .. gridW .. "x" .. gridH, vx + 8, vy + 22)
  love.graphics.setColor(0.8, 0.6, 0.2)
  love.graphics.print("Screens: " .. #world.cells, vx + 8, vy + 38)

  -- Scale screens in 4x3 steps (4x3, 8x6, 12x9, 16x12, ...) so they stay
  -- inside the card with a bit of bottom margin.
  local baseW, baseH = 4, 3
  local padding = 10
  local bottomMargin = 16
  local textHeaderHeight = 56

  local availW = vw - padding * 2
  local availH = vh - textHeaderHeight - bottomMargin
  local maxScaleW = math.floor(availW / (gridW * baseW))
  local maxScaleH = math.floor(availH / (gridH * baseH))
  local scale = math.max(1, math.min(maxScaleW, maxScaleH))

  local cellW = baseW * scale
  local cellH = baseH * scale

  local gridPixelW = gridW * cellW
  local gridPixelH = gridH * cellH
  local offsetX = vx + math.floor((vw - gridPixelW) / 2)
  local contentTop = vy + textHeaderHeight
  local offsetY = contentTop + math.floor((availH - gridPixelH) / 2)

  love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
  for gy = 0, gridH - 1 do
    for gx = 0, gridW - 1 do
      local dx = offsetX + (gx * cellW)
      local dy = offsetY + (gy * cellH)
      love.graphics.rectangle("line", dx, dy, cellW, cellH)
    end
  end

  for _, cell in ipairs(world.cells) do
    if cell.x >= world.bounds.minX and cell.x <= world.bounds.maxX and
       cell.y >= world.bounds.minY and cell.y <= world.bounds.maxY then
      local drawX = offsetX + ((cell.x - world.bounds.minX) * cellW)
      local drawY = offsetY + ((cell.y - world.bounds.minY) * cellH)

      love.graphics.setColor(0.2, 0.6, 0.8, 0.6)
      love.graphics.rectangle("fill", drawX, drawY, cellW, cellH)

      love.graphics.setColor(0.4, 0.9, 1.0, 1.0)
      love.graphics.rectangle("line", drawX, drawY, cellW, cellH)
    end
  end
end

function Render.draw(worlds, canvas)
  love.graphics.setCanvas(canvas)
  love.graphics.clear(0.05, 0.05, 0.05)

  local viewW = CANVAS_W / COLS
  local viewH = CANVAS_H / ROWS

  love.graphics.setColor(1, 1, 1)
  love.graphics.printf(
    "Click or press\nany key to\nrandomize.\n\nESC to quit.",
    viewW * 3 + 10, viewH * 1 + 30, viewW - 20, "center"
  )

  for i, world in ipairs(worlds) do
    local col = (i - 1) % COLS
    local row = math.floor((i - 1) / COLS)
    drawWorldViewport(world, col * viewW, row * viewH, viewW, viewH)
  end

  love.graphics.setCanvas()
  love.graphics.setColor(1, 1, 1, 1)
  love.graphics.draw(canvas, 0, 0, 0, WINDOW_SCALE, WINDOW_SCALE)
end

return Render
