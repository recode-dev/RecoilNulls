"use strict";

function ofIterator(obj) {
  return obj[Symbol.iterator]();
}

function inIterator(obj) {
  return Object.keys(obj)[Symbol.iterator]();
}

function ofStep(it) {
  const r = it.next();
  return [r.value, r.done];
}

function inStep(it) {
  const r = it.next();
  return [r.value, r.done];
}

function getBestPath(arg0, arg1, { mathBindings }) {
  const self = this;
  const position = BattleCache.TickPosition || BattleCache.ServerPosition;
  const speed = BattleCache.CurrentSpeed / 20;
  let best = null;

  for (const key of Object.keys(BattleCache.Projectiles)) {
    const projectile = BattleCache.Projectiles[key];
    const damage = projectile.damage;
    if (typeof damage !== "number" || !Number.isFinite(damage) || damage < 0) {
      continue;
    }
  }

  for (let angle = 0; angle < 360; angle += 360 / 16) {
    const pathByAngle = self.getPathByAngle(position, angle, speed);
    const result = self.getPathDamageBounded(pathByAngle, arg1, best ? best.damage : null);
    if (result === null) {
      continue;
    }
    if (!best) {
      best = { angle, damage: result };
      continue;
    }
    if (result.damage < best.damage.damage) {
      best = { angle, damage: result };
      continue;
    }
    if (
      result.damage === best.damage.damage &&
      result.earliestTickEscaped < best.damage.earliestTickEscaped
    ) {
      best = { angle, damage: result };
    }
  }

  return best;
}

function getPathDamage({ capturedValue0, capturedValue1, capturedValue2, capturedValue3 }) {
  const position = BattleCache.CurrentPosition || BattleCache.ServerPosition;
  if (!position) {
    return false;
  }
  const minDistance = capturedValue1.MinDistance * 300;
  if (minDistance === 0) {
    return true;
  }
  const distance = capturedValue2.MathUtil.getDistance(
    position[0],
    position[1],
    capturedValue3[0],
    capturedValue3[1]
  );
  const check = distance > minDistance;
  capturedValue0._check = check;
  return check;
}

function getPathDamageBounded(pathByAngle, target, currentDamage) {
  if (!pathByAngle || !pathByAngle.points) {
    return null;
  }
  let damage = 0;
  let earliestTickEscaped = Infinity;
  for (const point of pathByAngle.points) {
    const radius = currentDamage && currentDamage.radius ? currentDamage.radius : 0;
    if (checkSweptCollision(point.start, point.end, target, target, radius)) {
      damage += point.damage;
      earliestTickEscaped = Math.min(earliestTickEscaped, point.tick);
    }
  }
  return { damage, earliestTickEscaped };
}

function getStillDamage(arg0, arg1, arg2, { dodgePlanner, mathBindings }) {
  const self = this;
  const [x, y] = arg1;
  if (typeof x !== "number" || typeof y !== "number") {
    return;
  }
  if (!Number.isFinite(x) || !Number.isFinite(y)) {
    return;
  }
  let damage = 0;
  const projectile = BattleCache.Projectiles[arg0];
  if (!projectile || projectile.isIndirect) {
    return;
  }
  if (!projectile.projectileTicks || !projectile.dodgeAllowed || projectile.isOwnTeam || projectile.state !== 0) {
    if (!projectile.areaEffectRadius) {
      return;
    }
  }
  if (projectile.isIndirect && !projectile.areaEffectRadius) {
    return;
  }
  if (BattleCache.CurrentTick < projectile.allowedTick) {
    return;
  }
  const ticks = projectile.projectileTicks;
  const keys = Object.keys(ticks);
  const lastKey = Number(keys[keys.length - 1]);
  const extraRadius = dodgePlanner.EXTRA_RADIUS;

  for (const key of keys) {
    const point = ticks[key];
    const same = ticks[Number(key)] === point;
    const px = point[0];
    const py = point[1];
    if (!same || typeof px !== "number" || typeof py !== "number") {
      continue;
    }
    const finite = px - px + py - py === 0;
    const dx = px - x;
    const dy = py - y;
    const distSq = dx * dx + dy * dy;
    const radius = arg2 + projectile.radius + extraRadius;

    if (projectile.isIndirect) {
      if (Number(key) !== lastKey) {
        continue;
      }
      const dist = mathBindings.MathUtil.getDistance(arg1[0], arg1[1], point[0], point[1]);
      if (dist <= arg2 + projectile.areaEffectRadius + extraRadius) {
        damage += projectile.damage;
        break;
      }
      continue;
    }

    if (self.checkSweptCollision(ticks[Number(key)], point, arg1, arg1, radius)) {
      damage += projectile.damage;
      if (!projectile.damageOnlyWithOneProj) {
        break;
      }
      const grouped = self.getGroupedProjectiles(Number(arg0));
      for (const other of grouped) {
        if (other === Number(arg0)) {
          continue;
        }
        if (distSq - radius * radius <= 0) {
          break;
        }
      }
      break;
    }

    if (Number(key) === lastKey && projectile.areaEffectRadius) {
      const aoeRadius = arg2 + projectile.areaEffectRadius + extraRadius;
      if (self.checkSweptCollision(ticks[Number(key)], point, arg1, arg1, aoeRadius)) {
        damage += projectile.damage;
        if (!projectile.damageOnlyWithOneProj) {
          break;
        }
        const grouped = self.getGroupedProjectiles(Number(arg0));
        for (const other of grouped) {
          if (other === Number(arg0)) {
            continue;
          }
          if (distSq - aoeRadius * aoeRadius <= 0) {
            break;
          }
        }
        break;
      }
    }
  }

  return damage;
}

function getStillDamageOriginal(arg0, arg1) {
  function getAngleDifference(a, b) {
    const diff = Math.abs((a - b) % 360);
    return diff > 180 ? 360 - diff : diff;
  }
  return getAngleDifference(arg0, arg1);
}

function getDamage(arg0, { configuration, dodgePlanner, mathBindings }) {
  const self = this;
  const position = BattleCache.ServerPosition;
  let total = 0;
  const seen = [];

  for (const key of Object.keys(BattleCache.Projectiles)) {
    const projectile = BattleCache.Projectiles[key];
    if (projectile.isIndirect) {
      continue;
    }
    if (!projectile.projectileTicks) {
      if (!projectile.areaEffectRadius) {
        continue;
      }
    } else if (!projectile.dodgeAllowed || projectile.isOwnTeam || projectile.state !== 0) {
      if (!projectile.areaEffectRadius) {
        continue;
      }
    }
    if (seen.includes(Number(key))) {
      continue;
    }
    if (BattleCache.CurrentTick < projectile.allowedTick) {
      continue;
    }
    const ticks = projectile.projectileTicks;
    const keys = Object.keys(ticks);
    const lastKey = Number(keys[keys.length - 1]);
    const extraRadius = dodgePlanner.EXTRA_RADIUS;

    for (const tickKey of keys) {
      const point = ticks[tickKey];

      if (projectile.isIndirect) {
        if (Number(tickKey) !== lastKey) {
          continue;
        }
        const dist = mathBindings.MathUtil.getDistance(position[0], position[1], point[0], point[1]);
        if (dist <= arg0 + projectile.areaEffectRadius + extraRadius) {
          total += projectile.damage;
          break;
        }
        continue;
      }

      if (
        self.checkSweptCollision(
          ticks[Number(tickKey)],
          point,
          position,
          position,
          arg0 + projectile.radius + extraRadius
        )
      ) {
        total += projectile.damage;
        if (!projectile.damageOnlyWithOneProj) {
          break;
        }
        const grouped = self.getGroupedProjectiles(Number(key));
        for (const other of grouped) {
          if (other === Number(key)) {
            continue;
          }
        }
        break;
      }

      if (Number(tickKey) === lastKey && projectile.areaEffectRadius) {
        if (
          self.checkSweptCollision(
            ticks[Number(tickKey)],
            point,
            position,
            position,
            arg0 + projectile.areaEffectRadius + extraRadius
          )
        ) {
          total += projectile.damage;
          if (!projectile.damageOnlyWithOneProj) {
            break;
          }
          const grouped = self.getGroupedProjectiles(Number(key));
          for (const other of grouped) {
            if (other === Number(key)) {
              continue;
            }
          }
          break;
        }
      }
    }
  }

  return total;
}

function getProjectilePathPoint({ capturedValue0, capturedValue1, capturedValue2, capturedValue3 }) {
  if (!capturedValue0.projectileTicks && !capturedValue0.isDoNotRotateClip) {
    return;
  }
  if (capturedValue0.rotates) {
    return;
  }
  const age = BattleCache.CurrentTick - capturedValue0.startTick;
  if (age <= 1 && !capturedValue0.isDoNotRotateClip) {
    return;
  }
  if (
    capturedValue0.startPosition[0] !== capturedValue1[0] ||
    capturedValue0.startPosition[1] !== capturedValue1[1]
  ) {
    if (!capturedValue0.dodgeAllowed || capturedValue0.isOwnTeam) {
      return;
    }
    if (!capturedValue0.isIndirect) {
      return;
    }
    return capturedValue3.deferProjectileTicks(
      capturedValue0.range,
      capturedValue0.speed,
      capturedValue0.position,
      capturedValue0.startAngle,
      !capturedValue0.passesEnvironment
    );
  }
}

function getProjectileTicks(arg0, arg1, arg2, arg3, arg4, { mathBindings, dodgePlanner }) {
  const ticks = [];
  const step = arg1 / 20;
  const count = Math.ceil(arg0 / step);
  const radians = mathBindings.MathUtil.getRadians(arg3);
  const cos = Math.cos(radians);
  const sin = Math.sin(radians);
  let position = arg2;

  for (let i = 1; i <= count; i++) {
    const distance = Math.min(step, arg0 - (i - 1) * step);
    const next = [
      position[0] + cos * distance,
      position[1] + sin * distance
    ];
    if (arg4 && dodgePlanner.checkWallCollisions(position, next)) {
      break;
    }
    ticks.push(next);
    position = next;
  }

  return ticks;
}

function getProjectilePosition(arg0, { capturedValue0, capturedValue1, capturedValue2, capturedValue3 }) {
  const logic = capturedValue0.LogicGameObject;
  if (
    this.getX === capturedValue1 &&
    this.getY === capturedValue2 &&
    this.getZ === capturedValue3 &&
    Number.isSafeInteger(logic.X) &&
    logic.X >= 0 &&
    logic.Y === logic.X + 4 &&
    logic.Z === logic.X + 8
  ) {
    const bytes = this.instance.add(logic.X).readByteArray(8);
    if (bytes && bytes.byteLength === 8) {
      const view = new DataView(bytes);
      if (arg0) {
        return [view.getInt32(0, true), view.getInt32(4, true), view.getInt32(8, true)];
      }
      return [view.getInt32(0, true), view.getInt32(4, true)];
    }
  }
  if (arg0) {
    return [this.getX(), this.getY(), this.getZ()];
  }
  return [this.getX(), this.getY()];
}

function getGroupedProjectiles(arg0) {
  const projectile = BattleCache.Projectiles[arg0];
  const result = [arg0];
  let i = arg0 - 1;
  while (closure1.call(undefined, i)) {
    result.push(i);
    i -= 1;
  }
  i = arg0 + 1;
  while (closure1.call(undefined, i)) {
    result.push(i);
    i += 1;
  }
  return result;
}

function getDamageOnlyWithOneProj({ capturedValue0 }) {
  const instance = this.instance;
  const value = instance.add(capturedValue0.LogicProjectileData.DamageOnlyWithOneProj).readU8();
  return !value;
}

function getMaxRange(arg0, { capturedValue0 }) {
  const entry = capturedValue0[arg0] || {};
  if (entry.Range) {
    return entry.Range;
  }
  if (GlobalCache.CastingRanges[arg0]) {
    return GlobalCache.CastingRanges[arg0];
  }
  if (entry.ProjectileRange && GlobalCache.CastingRanges[entry.ProjectileRange]) {
    return GlobalCache.CastingRanges[entry.ProjectileRange];
  }
  return undefined;
}

function getConstantFlyTime({ capturedValue0 }) {
  const instance = this.instance;
  return instance.add(capturedValue0.LogicProjectileData.ConstantFlyTime).readU8();
}

function getCollisionRadius({ capturedValue0 }) {
  const instance = this.instance;
  return instance.add(capturedValue0.LogicCharacterData.CollisionRadius).readInt();
}

function getBoomerangType({ capturedValue0 }) {
  const instance = this.instance;
  return instance.add(capturedValue0.LogicProjectileData.BoomerangType).readInt();
}

function getMirrored({ capturedValue0 }) {
  return base.add(capturedValue0.Mirrored).readU8();
}

function isBouncing({ capturedValue0 }) {
  const instance = this.instance;
  const value = instance.add(capturedValue0.LogicProjectileData.Bouncing).readU8();
  return !value;
}

function isIndirect() {
  const chargeType = this.getChargeType();
  const projectile = this.getProjectile();
  const nullProjectile = projectile.instance.isNull();
  if (nullProjectile) {
    if ([20, 26, 12, 10, 18].includes(chargeType)) {
      return this.getProjectile().instance.isNull();
    }
    return false;
  }
  if (!projectile.getIndirect()) {
    if ([20, 26, 12, 10, 18].includes(chargeType)) {
      return this.getProjectile().instance.isNull();
    }
    return false;
  }
  return true;
}

function getIndirect({ capturedValue0 }) {
  const instance = this.instance;
  const value = instance.add(capturedValue0.LogicProjectileData.Indirect).readInt();
  return !value;
}

function getPassesEnvironment({ capturedValue0 }) {
  const instance = this.instance;
  const first = instance.add(capturedValue0.LogicProjectileData.PiercesEnvironment).readU8();
  if (!first) {
    return true;
  }
  const second = instance.add(capturedValue0.LogicProjectileData.PiercesEnvironmentLikeButter).readU8();
  if (!second) {
    return true;
  }
  const third = instance.add(capturedValue0.LogicProjectileData.PassesEnvironment).readU8();
  return !third;
}

function getSpawnAreaEffectObject({ capturedValue0, capturedValue1 }) {
  const instance = this.instance;
  const pointer = instance.add(capturedValue1.LogicProjectileData.SpawnAreaEffectObject).readPointer();
  return Reflect.construct(capturedValue0, [pointer], capturedValue0);
}

function getSteeringLifeTime({ capturedValue0 }) {
  const instance = this.instance;
  return instance.add(capturedValue0.LogicProjectileData.SteeringLifeTime).readInt();
}

function getTotalDelta({ capturedValue0 }) {
  const instance = this.instance;
  return instance.add(capturedValue0.LogicProjectile.TotalDelta).readInt();
}

function getAngleDifference(arg0, arg1) {
  const diff = Math.abs((arg0 - arg1) % 360);
  return diff > 180 ? 360 - diff : diff;
}

function getRadians(arg0) {
  return arg0 * (Math.PI / 180);
}

function getDegreesToPoint(arg0, arg1, arg2, arg3) {
  const radians = this.getRadiansBetweenPoints(arg0, arg1, arg2, arg3);
  return this.getDegrees(radians);
}

function getDistance(originX, originY, targetX, targetY) {
  const dx = targetX - originX;
  const dy = targetY - originY;
  return Math.sqrt(dx * dx + dy * dy);
}

function checkSweptCollision(startPosition, endPosition, projectileStart, projectileEnd, combinedRadius) {
  const relativeX = startPosition[0] - projectileStart[0];
  const relativeY = startPosition[1] - projectileStart[1];
  const relativeVelocityX =
    endPosition[0] - startPosition[0] - (projectileEnd[0] - projectileStart[0]);
  const relativeVelocityY =
    endPosition[1] - startPosition[1] - (projectileEnd[1] - projectileStart[1]);

  const initialSeparation =
    relativeX * relativeX +
    relativeY * relativeY -
    combinedRadius * combinedRadius;

  if (initialSeparation <= 0) {
    return true;
  }

  const relativeSpeedSquared =
    relativeVelocityX * relativeVelocityX +
    relativeVelocityY * relativeVelocityY;

  if (relativeSpeedSquared === 0) {
    return false;
  }

  const quadraticLinear = 2 * (relativeX * relativeVelocityX + relativeY * relativeVelocityY);
  const discriminant =
    quadraticLinear * quadraticLinear -
    4 * relativeSpeedSquared * initialSeparation;

  if (discriminant < 0) {
    return false;
  }

  const sqrtDiscriminant = Math.sqrt(discriminant);
  const enterTime = (-quadraticLinear - sqrtDiscriminant) / (2 * relativeSpeedSquared);
  const exitTime = (-quadraticLinear + sqrtDiscriminant) / (2 * relativeSpeedSquared);

  return exitTime >= 0 && enterTime <= 1;
}

function getDynamicDamage(arg0, arg1) {
  const position = BattleCache.TickPosition || BattleCache.ServerPosition;
  const speed = BattleCache.CurrentSpeed / 20;
  const pathByAngle = this.getPathByAngle(position, arg0, speed);
  return this.getPathDamage(pathByAngle, arg1);
}

function hasResult() {
  return this.completed !== null;
}

function isCharacterPositionBased() {
  const baseName = this.getBaseName();
  if (baseName.includes("Circling")) {
    return true;
  }
  if (baseName === "AxeJugglerProjectile2") {
    return true;
  }
  return baseName === "AxeJugglerOverchargedProjectile2";
}

function f64Sqrt(arg0, { capturedValue0, capturedValue1 }) {
  const self = this;
  let fn = capturedValue0.get(self);
  if (!fn) {
    fn = capturedValue1(self.ORDER);
    capturedValue0.set(self, fn);
  }
  return fn(self, arg0);
}
