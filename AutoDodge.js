/**
 * AutoDodge — читаемая версия восстановленного модуля игры (Nulls Brawl, JS-бандл).
 *
 * Что это: логика клиентского автододжа — оценка урона по траектории,
 * выбор лучшего направления побега и инжект движения через тачи.
 *
 * Как читать: функции с `switch (pc)` — это восстановленный поток управления
 * (метки = адреса переходов байткода). Всё остальное — обычный JS.
 * `CALL`/`ofStep`/`inStep` — книжный учёт стека/итераторов, оставленный
 * из восстановителя; на логику не влияют.
 *
 * Что выкинуто как не относящееся к доджу:
 *   Map, poll, close, getRandomInRange, replace, includes, getBaseName,
 *   function_0ce1e4c1, function_0fc0cd06, function_8c10ca76, function_33c81158,
 *   function_578cc2be, function_585f09dc, function_a6c866cd, function_aef0610c,
 *   function_d63f5d05, function_e3bb6fd8, function_eb4fcd20,
 *   а также обвязка восстановителя: read/write/take/prepare/namedCells,
 *   metadata-объекты, каталог registry и тестовые экспорты.
 *
 * Имена: argN/localValueN — исходные слоты; config/refs — захваченные биндинги.
 */

/** Фабрика контроллера доджа: config, planner, подписка на тик. */
function createDodgeController(arg0, {mathBindings, captured_getCodeName_getOriginalCodeName_getRealName, configuration, captured_getDamage_getMaxRange, capturedValue4, captured_isOwn_isOwnTeam, dodgePlanner}) {
  let angle, angleDifference, baseName, damageOnlyWithOneProj, data, globalID, index, indirect, isBouncingResult, isDoNotRotateClipResult, maxRange, passesEnvironment, position, spawnAreaEffectObject, totalDelta, v0, v11, v17, v19, v20, v22, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v4, v40, v41, v43, v44, v5, v6;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v44 = this;
    v25 = frame.makeClosure(1, {args, locals, refs, frame});
    v26 = frame.makeClosure(2, {args, locals, refs, frame});
    /* // Preserve the receiver for a method call. */
    if (!!configuration.isModEnabled("AutoDodge")) { pc = 137; continue; }
    pc = 136; continue;
  }
    case 136: {
    return;
  }
    case 137: {
    if (!!BattleCache["Projectiles"]) { pc = 152; continue; }
    pc = 151; continue;
  }
    case 151: {
    return;
  }
    case 152: {
    v0 = Reflect.construct([captured_getDamage_getMaxRange][0], [arg0], [captured_getDamage_getMaxRange][0]);
    /* // Preserve the receiver for a method call. */
    globalID = v0.getGlobalID();
    if (!BattleCache["Projectiles"][globalID]) { pc = 192; continue; }
    pc = 191; continue;
  }
    case 191: {
    return;
  }
    case 192: {
    /* // Preserve the receiver for a method call. */
    data = v0.getData();
    /* // Preserve the receiver for a method call. */
    baseName = data.getBaseName();
    /* // Preserve the receiver for a method call. */
    if (!baseName.includes("Overcharged")) { pc = 257; continue; }
    pc = 237; continue;
  }
    case 237: {
    /* // Preserve the receiver for a method call. */
    pc = 260; continue;
  }
    case 257: {
    pc = 260; continue;
  }
    case 260: {
    v4 = baseName;
    /* // Preserve the receiver for a method call. */
    if (!data.isCharacterPositionBased()) { pc = 277; continue; }
    pc = 276; continue;
  }
    case 276: {
    return;
  }
    case 277: {
    /* // Preserve the receiver for a method call. */
    if (!data.getSteeringLifeTime()) { pc = 292; continue; }
    pc = 291; continue;
  }
    case 291: {
    return;
  }
    case 292: {
    /* // Preserve the receiver for a method call. */
    if (!data.getConstantFlyTime()) { pc = 307; continue; }
    pc = 306; continue;
  }
    case 306: {
    return;
  }
    case 307: {
    /* // Preserve the receiver for a method call. */
    if (!data.getBoomerangType()) { pc = 338; continue; }
    pc = 321; continue;
  }
    case 321: {
    /* // Preserve the receiver for a method call. */
    if (!baseName.includes(constant(0))) { pc = 338; continue; }
    pc = 337; continue;
  }
    case 337: {
    return;
  }
    case 338: {
    if ([capturedValue4[v4]][0]) { pc = 350; continue; }
    pc = 348; continue;
  }
    case 348: {
    pc = 350; continue;
  }
    case 350: {
    v5 = {};
    if (!v5["Disabled"]) { pc = 364; continue; }
    pc = 363; continue;
  }
    case 363: {
    return;
  }
    case 364: {
    v6 = BattleCache["CurrentTick"];
    /* // Preserve the receiver for a method call. */
    position = v0.getPosition();
    /* // Preserve the receiver for a method call. */
    angle = v0.getAngle();
    /* // Preserve the receiver for a method call. */
    totalDelta = v0.getTotalDelta();
    /* // Preserve the receiver for a method call. */
    index = v0.getIndex();
    if ([v5["Speed"]][0]) { pc = 458; continue; }
    pc = 445; continue;
  }
    case 445: {
    /* // Preserve the receiver for a method call. */
    pc = 458; continue;
  }
    case 458: {
    v11 = data.getSpeed();
    if ([v5["Radius"]][0]) { pc = 495; continue; }
    pc = 472; continue;
  }
    case 472: {
    /* // Preserve the receiver for a method call. */
    pc = 495; continue;
  }
    case 495: {
    /* // Preserve the receiver for a method call. */
    isDoNotRotateClipResult = data.isDoNotRotateClip();
    /* // Preserve the receiver for a method call. */
    indirect = data.getIndirect();
    /* // Preserve the receiver for a method call. */
    isBouncingResult = data.isBouncing();
    /* // Preserve the receiver for a method call. */
    damageOnlyWithOneProj = data.getDamageOnlyWithOneProj();
    /* // Preserve the receiver for a method call. */
    passesEnvironment = data.getPassesEnvironment();
    if (!indirect) { pc = 572; continue; }
    pc = 571; continue;
  }
    case 571: {
    return;
  }
    case 572: {
    /* // Preserve the receiver for a method call. */
    if ([captured_getDamage_getMaxRange.getDamage(v4)][0]) { pc = 592; continue; }
    pc = 590; continue;
  }
    case 590: {
    pc = 592; continue;
  }
    case 592: {
    v17 = 1;
    /* // Preserve the receiver for a method call. */
    maxRange = captured_getDamage_getMaxRange.getMaxRange(v4);
    if (!!maxRange) { pc = 618; continue; }
    pc = 617; continue;
  }
    case 617: {
    return;
  }
    case 618: {
    v19 = totalDelta / 1000;
    if ([v5["ExtraRange"]][0]) { pc = 654; continue; }
    pc = 652; continue;
  }
    case 652: {
    pc = 654; continue;
  }
    case 654: {
    v20 = maxRange * 1 - v19 + 200 + 0;
    /* // Preserve the receiver for a method call. */
    spawnAreaEffectObject = data.getSpawnAreaEffectObject();
    /* // Preserve the receiver for a method call. */
    if (!spawnAreaEffectObject["instance"].isNull()) { pc = 694; continue; }
    pc = 691; continue;
  }
    case 691: {
    pc = 706; continue;
  }
    case 694: {
    /* // Preserve the receiver for a method call. */
    pc = 706; continue;
  }
    case 706: {
    v22 = spawnAreaEffectObject.getRadius();
    /* // Preserve the receiver for a method call. */
    config = configuration.getConfig("AutoDodge");
    v24 = ![];
    if (![v24][0]) { pc = 747; continue; }
    pc = 743; continue;
  }
    case 743: {
    pc = 747; continue;
  }
    case 747: {
    if (![v24][0]) { pc = 786; continue; }
    pc = 754; continue;
  }
    case 754: {
    /* // Preserve the receiver for a method call. */
    v24 = [Math.random() * 100 < config["Chance"]][0];
    pc = 786; continue;
  }
    case 786: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    v27 = [Math.round(config["Reaction"][0] / 50), Math.round(config["Reaction"][1] / 50)];
    /* // Preserve the receiver for a method call. */
    v28 = mathBindings["MathUtil"].getRandomInRange(v27[0], v27[1]);
    v29 = v6 + v28;
    v30 = 0;
    pc = 893; continue;
  }
    case 893: {
    pc = 1000; continue;
  }
    case 907: {
    v31 = inIterator(BattleCache["Projectiles"]);
    pc = 911; continue;
  }
    case 911: {
    v32 = BattleCache["Projectiles"][v31];
    if (![v32["index"] == index][0]) { pc = 999; continue; }
    pc = 947; continue;
  }
    case 947: {
    if (![v32["startTick"] == v6][0]) { pc = 999; continue; }
    pc = 964; continue;
  }
    case 964: {
    if (![v32["order"] + 1 > v30][0]) { pc = 999; continue; }
    pc = 983; continue;
  }
    case 983: {
    v30 = [v32["order"] + 1][0];
    pc = 999; continue;
  }
    case 999: {
    pc = 1000; continue;
  }
    case 1000: {
    const [t1, t2] = inStep(0);
    if (!t2) { pc = 907; continue; }
    pc = 1003; continue;
  }
    case 1003: {
    v33 = BattleCache["ProjectilesUpdatedThisTick"] < v44["MAX_THREAD_PROJECTILES"];
    if (![!indirect][0]) { pc = 1039; continue; }
    pc = 1034; continue;
  }
    case 1034: {
    pc = 1039; continue;
  }
    case 1039: {
    v34 = !passesEnvironment;
    if (!v33) { pc = 1074; continue; }
    pc = 1046; continue;
  }
    case 1046: {
    /* // Preserve the receiver for a method call. */
    pc = 1075; continue;
  }
    case 1074: {
    pc = 1075; continue;
  }
    case 1075: {
    v35 = null;
    v36 = ![];
    /* [BattleCache, propertyKey("ProjectilesUpdatedThisTick")][0][[BattleCache, propertyKey("ProjectilesUpdatedThisTick")][1]] = [BattleCache, propertyKey("ProjectilesUpdatedThisTick")][0][[BattleCache, propertyKey("ProjectilesUpdatedThisTick")][1]] + 1 */
    if ([v33][0]) { pc = 1129; continue; }
    pc = 1105; continue;
  }
    case 1105: {
    /* // Preserve the receiver for a method call. */
    pc = 1129; continue;
  }
    case 1129: {
    if (![!v33][0]) { pc = 1156; continue; }
    pc = 1137; continue;
  }
    case 1137: {
    pc = 1156; continue;
  }
    case 1156: {
    if ([[v20, v11, position, angle, v34]][0]) { pc = 1161; continue; }
    pc = 1159; continue;
  }
    case 1159: {
    pc = 1161; continue;
  }
    case 1161: {
    v37 = null;
    if (!!isDoNotRotateClipResult) { pc = 1327; continue; }
    pc = 1172; continue;
  }
    case 1172: {
    if (!v35) { pc = 1327; continue; }
    pc = 1180; continue;
  }
    case 1180: {
    /* // Preserve the receiver for a method call. */
    v38 = Object.keys(v35);
    v39 = v38[v38["length"] - 1];
    v40 = v35[Number.call(undefined, v39)];
    /* // Preserve the receiver for a method call. */
    v41 = mathBindings["MathUtil"].getDegreesToPoint(position[0], position[1], v40[0], v40[1]);
    /* // Preserve the receiver for a method call. */
    angleDifference = mathBindings["MathUtil"].getAngleDifference(angle, v41);
    if (![angleDifference > 1][0]) { pc = 1326; continue; }
    pc = 1316; continue;
  }
    case 1316: {
    v36 = [!![]][0];
    pc = 1326; continue;
  }
    case 1326: {
    pc = 1327; continue;
  }
    case 1327: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    v43 = {globalID: globalID, index: index, order: v30, name: baseName, startTick: v6, startPosition: position, startAngle: angle, startTotalDelta: totalDelta, state: v0.getState(), position: position, isOwn: captured_isOwn_isOwnTeam.isOwn(index), isOwnTeam: captured_isOwn_isOwnTeam.isOwnTeam(index), radius: data.getRadius(), totalDelta: totalDelta, range: v20, isDoNotRotateClip: isDoNotRotateClipResult, setFirstAngle: ![], endTick: null, areaEffectRadius: v22, isIndirect: indirect, isBouncing: isBouncingResult, damageOnlyWithOneProj: damageOnlyWithOneProj, passesEnvironment: passesEnvironment, damage: v17, rotates: v36, speed: v11, maxRange: maxRange, projectileTicks: v35, threadProjectileData: v37, dodgeAllowed: v24, allowedTick: v29, ignoreExtraRadius: dodgePlanner["IGNORE_EXTRA_RADIUS"].includes(v4)};
    /* BattleCache["Projectiles"][propertyKey(globalID)] = v43 */
    return;
  }
    }
  }
}

/** Основной шаг доджа: собрать угрозы, выбрать направление, отправить движение. */
function updateDodge(arg0, {mathBindings, capturedValue1, configuration, captured_getDamage_getMaxRange, capturedValue4, capturedValue5}) {
  let globalID, position, state, v0, v2, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v9 = frame.makeClosure(0, {args, locals, refs, frame});
    /* // Preserve the receiver for a method call. */
    if (!!configuration.isModEnabled("AutoDodge")) { pc = 52; continue; }
    pc = 51; continue;
  }
    case 51: {
    return;
  }
    case 52: {
    if (!!BattleCache["Projectiles"]) { pc = 67; continue; }
    pc = 66; continue;
  }
    case 66: {
    return;
  }
    case 67: {
    if (!BattleCache["LastTick"] === BattleCache["CurrentTick"]) { pc = 93; continue; }
    pc = 92; continue;
  }
    case 92: {
    return;
  }
    case 93: {
    v0 = Reflect.construct([captured_getDamage_getMaxRange][0], [arg0], [captured_getDamage_getMaxRange][0]);
    /* // Preserve the receiver for a method call. */
    globalID = v0.getGlobalID();
    v2 = BattleCache["Projectiles"][globalID];
    if (!!v2) { pc = 138; continue; }
    pc = 137; continue;
  }
    case 137: {
    return;
  }
    case 138: {
    /* // Preserve the receiver for a method call. */
    position = v0.getPosition();
    /* // Preserve the receiver for a method call. */
    state = v0.getState();
    v5 = v2["state"];
    /* v2[propertyKey("state")] = state */
    /* v2[propertyKey("position")] = position */
    if ([capturedValue4[v2["name"]]][0]) { pc = 220; continue; }
    pc = 218; continue;
  }
    case 218: {
    pc = 220; continue;
  }
    case 220: {
    v6 = {};
    /* // Preserve the receiver for a method call. */
    v7 = v0.getTotalDelta() / 1000;
    if ([v6["ExtraRange"]][0]) { pc = 273; continue; }
    pc = 271; continue;
  }
    case 271: {
    pc = 273; continue;
  }
    case 273: {
    v8 = v2["maxRange"] * 1 - v7 + 200 + 0;
    /* v2[propertyKey("range")] = v8 */
    if (![state != capturedValue5["Active"]][0]) { pc = 343; continue; }
    pc = 309; continue;
  }
    case 309: {
    if (![v5 != state][0]) { pc = 343; continue; }
    pc = 320; continue;
  }
    case 320: {
    /* [v2, propertyKey("endTick"), BattleCache["CurrentTick"]][0][[v2, propertyKey("endTick"), BattleCache["CurrentTick"]][1]] = [v2, propertyKey("endTick"), BattleCache["CurrentTick"]][2] */
    pc = 343; continue;
  }
    case 343: {
    if (!state != capturedValue5["Active"]) { pc = 369; continue; }
    pc = 359; continue;
  }
    case 359: {
    if (!v5 == state) { pc = 369; continue; }
    pc = 368; continue;
  }
    case 368: {
    return;
  }
    case 369: {
    return;
  }
    }
  }
}

/** Работа с тачами: инжект/релиз, возврат стика в исходное положение. */
function updateTouches({touchController, dodgePlanner, joystickState, inputActions, touchStorage, configuration, predictionWorker, captured_getInstance, skillController, mathBindings, predictionState}) {
  let collisionRadius, currentCharacter, damage, data, dynamicDamage, mirrored, radians, v0, v1, v12, v13, v14, v15, v16, v17, v18, v21, v22, v23, v24, v25, v26, v27, v28, v3, v30, v31, v32, v33, v4, v5, v6, v7;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    if (!BattleCache["GoingBackToOriginalDodge"]) { pc = 217; continue; }
    pc = 22; continue;
  }
    case 22: {
    /* // Preserve the receiver for a method call. */
    v0 = joystickState.getTouchIdBySide(inputActions["Move"], !![]);
    if (!v0 == undefined) { pc = 77; continue; }
    pc = 60; continue;
  }
    case 60: {
    /* BattleCache[propertyKey("GoingBackToOriginalDodge")] = ![] */
    return;
  }
    case 77: {
    /* // Preserve the receiver for a method call. */
    v1 = touchStorage.findStoredTouch(v0);
    if (!v1 == undefined) { pc = 117; continue; }
    pc = 100; continue;
  }
    case 100: {
    /* BattleCache[propertyKey("GoingBackToOriginalDodge")] = ![] */
    return;
  }
    case 117: {
    if (!v1["animateData"]) { pc = 179; continue; }
    pc = 128; continue;
  }
    case 128: {
    /* v1["animateData"][propertyKey("targetX")] = v1["originalX"] */
    /* v1["animateData"][propertyKey("targetY")] = v1["originalY"] */
    return;
  }
    case 179: {
    /* // Preserve the receiver for a method call. */
    return;
  }
    case 217: {
    /* // Preserve the receiver for a method call. */
    if (!!configuration.isModEnabled("AutoDodge")) { pc = 238; continue; }
    pc = 237; continue;
  }
    case 237: {
    return;
  }
    case 238: {
    if (!BattleCache["CurrentTick"] == BattleCache["LastTick"]) { pc = 290; continue; }
    pc = 263; continue;
  }
    case 263: {
    /* // Preserve the receiver for a method call. */
    if (!!predictionWorker.hasResult()) { pc = 290; continue; }
    pc = 278; continue;
  }
    case 278: {
    return BattleCache["AllowedLerpingDodge"];
  }
    case 290: {
    /* // Preserve the receiver for a method call. */
    config = configuration.getConfig("AutoDodge");
    /* // Preserve the receiver for a method call. */
    v3 = joystickState.getTouchIdBySide(inputActions["Move"], !![]);
    if (!v3 != undefined) { pc = 1698; continue; }
    pc = 343; continue;
  }
    case 343: {
    /* // Preserve the receiver for a method call. */
    v4 = touchStorage.findStoredTouch(v3);
    if ([!v4][0]) { pc = 480; continue; }
    pc = 457; continue;
  }
    case 457: {
    if (!!v4["active"]) { pc = 483; continue; }
    pc = 470; continue;
  }
    case 470: {
    pc = 480; continue;
  }
    case 480: {
    if (!!v4["fake"]) { pc = 483; continue; }
    pc = 482; continue;
  }
    case 482: {
    return;
  }
    case 483: {
    if ([BattleCache["TickPosition"]][0]) { pc = 509; continue; }
    pc = 497; continue;
  }
    case 497: {
    pc = 509; continue;
  }
    case 509: {
    v5 = BattleCache["ServerPosition"];
    v6 = BattleCache["CurrentSpeed"];
    if ([!v5][0]) { pc = 536; continue; }
    pc = 531; continue;
  }
    case 531: {
    pc = 536; continue;
  }
    case 536: {
    if (!!v6) { pc = 539; continue; }
    pc = 538; continue;
  }
    case 538: {
    return;
  }
    case 539: {
    v7 = Reflect.construct([captured_getInstance][0], [], [captured_getInstance][0]);
    /* // Preserve the receiver for a method call. */
    currentCharacter = v7.getCurrentCharacter();
    /* // Preserve the receiver for a method call. */
    if (!currentCharacter["instance"].isNull()) { pc = 583; continue; }
    pc = 582; continue;
  }
    case 582: {
    return;
  }
    case 583: {
    /* // Preserve the receiver for a method call. */
    data = currentCharacter.getData();
    /* // Preserve the receiver for a method call. */
    collisionRadius = data.getCollisionRadius();
    /* // Preserve the receiver for a method call. */
    mirrored = v7.getMirrored();
    v12 = Reflect.construct([skillController][0], [], [skillController][0]);
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    v13 = [v12.getMoveStickBgX(), v12.getMoveStickBgY()];
    v14 = [v4["originalX"], v4["originalY"]];
    /* // Preserve the receiver for a method call. */
    v15 = mathBindings["MathUtil"].getDegreesToPoint(v13[0], v13[1], v14[0], v14[1]);
    if (![mirrored][0]) { pc = 748; continue; }
    pc = 732; continue;
  }
    case 732: {
    v15 = [v15 + 180 % 360][0];
    pc = 748; continue;
  }
    case 748: {
    /* // Preserve the receiver for a method call. */
    v16 = Date.now();
    if (!BattleCache["LastMoveAngle"] != undefined) { pc = 793; continue; }
    pc = 780; continue;
  }
    case 780: {
    pc = 796; continue;
  }
    case 793: {
    pc = 796; continue;
  }
    case 796: {
    v17 = v15;
    /* // Preserve the receiver for a method call. */
    v18 = predictionWorker.poll(BattleCache, config["Safety"], collisionRadius, v15, v17, v4["active"], v3, v16);
    /* // Preserve the receiver for a method call. */
    damage = dodgePlanner.getDamage(collisionRadius);
    /* // Preserve the receiver for a method call. */
    dynamicDamage = dodgePlanner.getDynamicDamage(v15, collisionRadius);
    if (!BattleCache["AllowedLerpingDodge"]) { pc = 940; continue; }
    pc = 916; continue;
  }
    case 916: {
    if (!v4["captured"]) { pc = 940; continue; }
    pc = 927; continue;
  }
    case 927: {
    pc = 941; continue;
  }
    case 940: {
    pc = 941; continue;
  }
    case 941: {
    v21 = undefined;
    v22 = undefined;
    /* setName(frame.makeClosure(1, {args, locals, refs, frame}), "") */
    v23 = frame.makeClosure(1, {args, locals, refs, frame});
    /* // Preserve the receiver for a method call. */
    v24 = Math.max(damage, dynamicDamage["damage"]);
    if (!v24 <= 0) { pc = 1051; continue; }
    pc = 990; continue;
  }
    case 990: {
    if (!BattleCache["DodgeReturnTimeout"]) { pc = 1050; continue; }
    pc = 1003; continue;
  }
    case 1003: {
    if (!v16 - BattleCache["DodgeReturnTimeout"] < config["ReturnDelay"]) { pc = 1050; continue; }
    pc = 1030; continue;
  }
    case 1030: {
    if ([v18 === undefined][0]) { pc = 1042; continue; }
    pc = 1037; continue;
  }
    case 1037: {
    pc = 1042; continue;
  }
    case 1042: {
    if (!v23.call(undefined)) { pc = 1050; continue; }
    pc = 1044; continue;
  }
    case 1044: {
    return !![];
  }
    case 1050: {
    return;
  }
    case 1051: {
    if (!v16 - BattleCache["DodgeChangeTimeout"] < config["ChangeDelay"]) { pc = 1098; continue; }
    pc = 1078; continue;
  }
    case 1078: {
    if ([v18 === undefined][0]) { pc = 1090; continue; }
    pc = 1085; continue;
  }
    case 1085: {
    pc = 1090; continue;
  }
    case 1090: {
    if (!v23.call(undefined)) { pc = 1098; continue; }
    pc = 1092; continue;
  }
    case 1092: {
    return !![];
  }
    case 1098: {
    if (!v18 === null) { pc = 1131; continue; }
    pc = 1104; continue;
  }
    case 1104: {
    if (!v23.call(undefined)) { pc = 1131; continue; }
    pc = 1110; continue;
  }
    case 1110: {
    /* BattleCache[propertyKey("DodgeReturnTimeout")] = v16 */
    return !![];
  }
    case 1131: {
    /* BattleCache["DodgeChangeTimeout"] = null */
    if ([damage > 0][0]) { pc = 1162; continue; }
    pc = 1151; continue;
  }
    case 1151: {
    pc = 1162; continue;
  }
    case 1162: {
    if (!!v4["active"]) { pc = 1192; continue; }
    pc = 1164; continue;
  }
    case 1164: {
    if (!BattleCache["LastMoveAngle"] != undefined) { pc = 1192; continue; }
    pc = 1179; continue;
  }
    case 1179: {
    pc = 1195; continue;
  }
    case 1192: {
    pc = 1195; continue;
  }
    case 1195: {
    v25 = v15;
    if (!v18) { pc = 1240; continue; }
    pc = 1202; continue;
  }
    case 1202: {
    /* // Preserve the receiver for a method call. */
    pc = 1241; continue;
  }
    case 1240: {
    pc = 1241; continue;
  }
    case 1241: {
    v26 = null;
    if (![v26][0] === undefined || [v26][0] === null) { pc = 1254; continue; }
    pc = 1250; continue;
  }
    case 1250: {
    pc = 1266; continue;
  }
    case 1254: {
    pc = 1266; continue;
  }
    case 1266: {
    if (!undefined["damage"]["damage"] === 0) { pc = 1275; continue; }
    pc = 1270; continue;
  }
    case 1270: {
    pc = 1293; continue;
  }
    case 1275: {
    /* // Preserve the receiver for a method call. */
    pc = 1293; continue;
  }
    case 1293: {
    v27 = dodgePlanner.getBestPath(v25, collisionRadius);
    if (!v27["damage"]["damage"] >= v24) { pc = 1317; continue; }
    pc = 1316; continue;
  }
    case 1316: {
    return;
  }
    case 1317: {
    if (![BattleCache["LastJoystickAngle"]][0]) { pc = 1392; continue; }
    pc = 1331; continue;
  }
    case 1331: {
    /* // Preserve the receiver for a method call. */
    if (![mathBindings["MathUtil"].getAngleDifference(BattleCache["LastJoystickAngle"], v27["angle"]) > 1][0]) { pc = 1392; continue; }
    pc = 1375; continue;
  }
    case 1375: {
    /* [BattleCache, propertyKey("DodgeChangeTimeout"), v16][0][[BattleCache, propertyKey("DodgeChangeTimeout"), v16][1]] = [BattleCache, propertyKey("DodgeChangeTimeout"), v16][2] */
    pc = 1392; continue;
  }
    case 1392: {
    /* BattleCache[propertyKey("LastJoystickAngle")] = v27["angle"] */
    v28 = 30;
    /* // Preserve the receiver for a method call. */
    radians = mathBindings["MathUtil"].getRadians(v27["angle"]);
    if (!mirrored) { pc = 1472; continue; }
    pc = 1452; continue;
  }
    case 1452: {
    /* // Preserve the receiver for a method call. */
    pc = 1489; continue;
  }
    case 1472: {
    /* // Preserve the receiver for a method call. */
    pc = 1489; continue;
  }
    case 1489: {
    v30 = Math.cos(radians);
    if (!mirrored) { pc = 1516; continue; }
    pc = 1496; continue;
  }
    case 1496: {
    /* // Preserve the receiver for a method call. */
    pc = 1533; continue;
  }
    case 1516: {
    /* // Preserve the receiver for a method call. */
    pc = 1533; continue;
  }
    case 1533: {
    v31 = Math.sin(radians);
    /* // Preserve the receiver for a method call. */
    v32 = Math.sqrt(v30 * v30 + v31 * v31);
    if (!v32 > 0) { pc = 1612; continue; }
    pc = 1573; continue;
  }
    case 1573: {
    pc = 1615; continue;
  }
    case 1612: {
    pc = 1615; continue;
  }
    case 1615: {
    v33 = v13;
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* BattleCache[propertyKey("AllowedLerpingDodge")] = !![] */
    /* BattleCache[propertyKey("DodgeReturnTimeout")] = v16 */
    return !![];
  }
    case 1698: {
    if (!!![]) { pc = 1706; continue; }
    pc = 1705; continue;
  }
    case 1705: {
    return;
  }
    case 1706: {
    return;
  }
    }
  }
}

/** Перебор направлений побега и оценка (damage / steering) по каждому. */
function getBestPath(arg0, arg1, {mathBindings}) {
  let pathByAngle, v0, v1, v10, v2, v3, v4, v5, v6, v7, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v10 = this;
    /* // Preserve the receiver for a method call. */
    if (!!Number.isFinite(arg0)) { pc = 46; continue; }
    pc = 33; continue;
  }
    case 33: {
    /* // Preserve the receiver for a method call. */
    /* return v10.getBestPathExhaustive(arg0, arg1) */
  }
    case 46: {
    pc = 51; continue;
  }
    case 51: {
    pc = 149; continue;
  }
    case 65: {
    v0 = inIterator(BattleCache["Projectiles"]);
    pc = 68; continue;
  }
    case 68: {
    v1 = BattleCache["Projectiles"][v0]["damage"];
    if ([typeof v1 !== "number"][0]) { pc = 134; continue; }
    pc = 106; continue;
  }
    case 106: {
    /* // Preserve the receiver for a method call. */
    if ([!Number.isFinite(v1)][0]) { pc = 134; continue; }
    pc = 128; continue;
  }
    case 128: {
    pc = 134; continue;
  }
    case 134: {
    if (!v1 < 0) { pc = 149; continue; }
    pc = 136; continue;
  }
    case 136: {
    /* // Preserve the receiver for a method call. */
    /* return v10.getBestPathExhaustive(arg0, arg1) */
  }
    case 149: {
    const [t1, t2] = inStep(/*iterator*/);
    if (!t2) { pc = 65; continue; }
    pc = 152; continue;
  }
    case 152: {
    if ([BattleCache["TickPosition"]][0]) { pc = 180; continue; }
    pc = 168; continue;
  }
    case 168: {
    pc = 180; continue;
  }
    case 180: {
    v2 = BattleCache["ServerPosition"];
    v3 = BattleCache["CurrentSpeed"] / 20;
    v4 = [];
    v5 = 0;
    pc = 207; continue;
  }
    case 207: {
    if (!v5 < 360) { pc = 251; continue; }
    pc = 216; continue;
  }
    case 216: {
    pc = 218; continue;
  }
    case 218: {
    /* // Preserve the receiver for a method call. */
    v5 = [v5 + 360 / 16][0];
    pc = 207; continue;
  }
    case 251: {
    /* // Preserve the receiver for a method call. */
    v6 = null;
    pc = 274; continue;
  }
    case 274: {
    pc = 517; continue;
  }
    case 281: {
    v7 = ofIterator(v4);
    pc = 285; continue;
  }
    case 285: {
    if (!v6) { pc = 337; continue; }
    pc = 296; continue;
  }
    case 296: {
    if (!v6["damage"]["damage"] === 0) { pc = 337; continue; }
    pc = 315; continue;
  }
    case 315: {
    if (!v6["damage"]["earliestTickEscaped"] === 0) { pc = 337; continue; }
    pc = 334; continue;
  }
    case 334: {
    pc = 525; continue;
  }
    case 337: {
    /* // Preserve the receiver for a method call. */
    pathByAngle = v10.getPathByAngle(v2, v7, v3);
    /* // Preserve the receiver for a method call. */
    if (![v6][0]) { pc = 387; continue; }
    pc = 377; continue;
  }
    case 377: {
    pc = 387; continue;
  }
    case 387: {
    v9 = v10.getPathDamageBounded(pathByAngle, arg1, v6["damage"]);
    if (!v9 === null) { pc = 400; continue; }
    pc = 398; continue;
  }
    case 398: {
    pc = 517; continue;
  }
    case 400: {
    if ([!v6][0]) { pc = 491; continue; }
    pc = 407; continue;
  }
    case 407: {
    if ([v9["damage"] < v6["damage"]["damage"]][0]) { pc = 491; continue; }
    pc = 436; continue;
  }
    case 436: {
    if (![v9["damage"] === v6["damage"]["damage"]][0]) { pc = 491; continue; }
    pc = 465; continue;
  }
    case 465: {
    pc = 491; continue;
  }
    case 491: {
    if (![v9["earliestTickEscaped"] < v6["damage"]["earliestTickEscaped"]][0]) { pc = 516; continue; }
    pc = 494; continue;
  }
    case 494: {
    v6 = [{angle: v7, damage: v9}][0];
    pc = 516; continue;
  }
    case 516: {
    pc = 517; continue;
  }
    case 517: {
    const [t3, t4] = ofStep(/*iterator*/);
    if (!t4) { pc = 281; continue; }
    pc = 524; continue;
  }
    case 524: {
    pc = 525; continue;
  }
    case 525: {
    return v6;
  }
    }
  }
}

/** Суммарный урон по пути из текущей позиции. */
function getPathDamage(arg0, arg1, {configuration, dodgePlanner, mathBindings}) {
  let v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v2, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v3, v30, v31, v32, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v32 = this;
    /* // Preserve the receiver for a method call. */
    config = configuration.getConfig("AutoDodge");
    v1 = arg1 + config["Safety"] * 10;
    v2 = 0;
    v3 = [];
    v4 = 0;
    v5 = 0;
    pc = 71; continue;
  }
    case 71: {
    pc = 1148; continue;
  }
    case 86: {
    v6 = inIterator(BattleCache["Projectiles"]);
    pc = 90; continue;
  }
    case 90: {
    v7 = BattleCache["Projectiles"][v6];
    if (!!![]) { pc = 155; continue; }
    pc = 141; continue;
  }
    case 141: {
    if (!v7["isIndirect"]) { pc = 155; continue; }
    pc = 152; continue;
  }
    case 152: {
    pc = 1148; continue;
  }
    case 155: {
    if ([!v7["projectileTicks"]][0]) { pc = 232; continue; }
    pc = 168; continue;
  }
    case 168: {
    if ([!v7["dodgeAllowed"]][0]) { pc = 232; continue; }
    pc = 182; continue;
  }
    case 182: {
    if ([v7["isOwnTeam"]][0]) { pc = 232; continue; }
    pc = 195; continue;
  }
    case 195: {
    if ([v7["state"] != 0][0]) { pc = 232; continue; }
    pc = 210; continue;
  }
    case 210: {
    if (!v7["isIndirect"]) { pc = 237; continue; }
    pc = 222; continue;
  }
    case 222: {
    pc = 232; continue;
  }
    case 232: {
    if (!!v7["areaEffectRadius"]) { pc = 237; continue; }
    pc = 234; continue;
  }
    case 234: {
    pc = 1148; continue;
  }
    case 237: {
    /* // Preserve the receiver for a method call. */
    if (!v3.includes(Number.call(undefined, v6))) { pc = 263; continue; }
    pc = 260; continue;
  }
    case 260: {
    pc = 1148; continue;
  }
    case 263: {
    v8 = BattleCache["CurrentTick"] >= v7["allowedTick"];
    if (!!v8) { pc = 295; continue; }
    pc = 292; continue;
  }
    case 292: {
    pc = 1148; continue;
  }
    case 295: {
    v9 = v7["projectileTicks"];
    /* // Preserve the receiver for a method call. */
    v10 = Object.keys(v9);
    v11 = Number.call(undefined, v10[v10["length"] - 1]);
    v12 = BattleCache["CurrentTick"] - v7["startTick"];
    /* // Preserve the receiver for a method call. */
    v13 = v32.getStillDamage(Number.call(undefined, v6), arg0[0], arg1);
    if (!v13) { pc = 405; continue; }
    pc = 402; continue;
  }
    case 402: {
    pc = 408; continue;
  }
    case 405: {
    pc = 408; continue;
  }
    case 408: {
    v14 = v1;
    if (!v7["ignoreExtraRadius"]) { pc = 424; continue; }
    pc = 421; continue;
  }
    case 421: {
    pc = 433; continue;
  }
    case 424: {
    pc = 433; continue;
  }
    case 433: {
    v15 = dodgePlanner["EXTRA_RADIUS"];
    if (!v7["isIndirect"]) { pc = 588; continue; }
    pc = 449; continue;
  }
    case 449: {
    v16 = v9[v11];
    v17 = v11 - v12;
    v18 = arg0[v17];
    if ([!v18][0]) { pc = 498; continue; }
    pc = 493; continue;
  }
    case 493: {
    pc = 498; continue;
  }
    case 498: {
    if (!!v16) { pc = 503; continue; }
    pc = 500; continue;
  }
    case 500: {
    pc = 1148; continue;
  }
    case 503: {
    /* // Preserve the receiver for a method call. */
    v19 = mathBindings["MathUtil"].getDistance(v18[0], v18[1], v16[0], v16[1]);
    if (!v19 <= v14 + v7["areaEffectRadius"] + v15) { pc = 1148; continue; }
    pc = 567; continue;
  }
    case 567: {
    v5 = [v5 + v7["damage"]][0];
    pc = 1155; continue;
  }
    case 588: {
    v20 = 1 / 0;
    v21 = 0;
    pc = 602; continue;
  }
    case 602: {
    if (!v21 < arg0["length"]) { pc = 1123; continue; }
    pc = 618; continue;
  }
    case 618: {
    pc = 620; continue;
  }
    case 620: {
    v22 = v21 + v2;
    v23 = v9[v22];
    if (!!v23) { pc = 662; continue; }
    pc = 659; continue;
  }
    case 659: {
    pc = 1112; continue;
  }
    case 662: {
    if (!v22 > 0) { pc = 694; continue; }
    pc = 669; continue;
  }
    case 669: {
    if (![v9[v22 - 1]][0] === undefined || [v9[v22 - 1]][0] === null) { pc = 703; continue; }
    pc = 682; continue;
  }
    case 682: {
    pc = 703; continue;
  }
    case 694: {
    pc = 703; continue;
  }
    case 703: {
    v24 = v7["startPosition"];
    v25 = arg0[v21];
    if (!v21 > 0) { pc = 728; continue; }
    pc = 719; continue;
  }
    case 719: {
    pc = 754; continue;
  }
    case 728: {
    if ([BattleCache["TickPosition"]][0]) { pc = 754; continue; }
    pc = 742; continue;
  }
    case 742: {
    pc = 754; continue;
  }
    case 754: {
    v26 = BattleCache["ServerPosition"];
    /* // Preserve the receiver for a method call. */
    if (!v32.checkSweptCollision(v24, v23, v26, v25, v14 + v7["radius"] + v15)) { pc = 895; continue; }
    pc = 798; continue;
  }
    case 798: {
    v5 = [v5 + v7["damage"]][0];
    if (!v7["damageOnlyWithOneProj"]) { pc = 1123; continue; }
    pc = 830; continue;
  }
    case 830: {
    /* // Preserve the receiver for a method call. */
    v27 = v32.getGroupedProjectiles(Number.call(undefined, v6));
    pc = 860; continue;
  }
    case 860: {
    pc = 886; continue;
  }
    case 866: {
    v28 = ofIterator(v27);
    pc = 870; continue;
  }
    case 870: {
    /* // Preserve the receiver for a method call. */
    pc = 886; continue;
  }
    case 886: {
    const [t1, t2] = ofStep(arg0[v21 - 1]);
    if (!t2) { pc = 866; continue; }
    pc = 890; continue;
  }
    case 890: {
    pc = 1123; continue;
  }
    case 895: {
    if (!v22 == v11) { pc = 1059; continue; }
    pc = 907; continue;
  }
    case 907: {
    if (!v7["areaEffectRadius"]) { pc = 1059; continue; }
    pc = 921; continue;
  }
    case 921: {
    /* // Preserve the receiver for a method call. */
    if (!v32.checkSweptCollision(v24, v23, v26, v25, v14 + v7["areaEffectRadius"] + v15)) { pc = 1059; continue; }
    pc = 963; continue;
  }
    case 963: {
    v5 = [v5 + v7["damage"]][0];
    if (!v7["damageOnlyWithOneProj"]) { pc = 1123; continue; }
    pc = 995; continue;
  }
    case 995: {
    /* // Preserve the receiver for a method call. */
    v29 = v32.getGroupedProjectiles(Number.call(undefined, v6));
    pc = 1025; continue;
  }
    case 1025: {
    pc = 1051; continue;
  }
    case 1031: {
    v30 = ofIterator(v29);
    pc = 1035; continue;
  }
    case 1035: {
    /* // Preserve the receiver for a method call. */
    pc = 1051; continue;
  }
    case 1051: {
    const [t3, t4] = ofStep(v7["startPosition"]);
    if (!t4) { pc = 1031; continue; }
    pc = 1055; continue;
  }
    case 1055: {
    pc = 1123; continue;
  }
    case 1059: {
    if (!v20 == 1 / 0) { pc = 1112; continue; }
    pc = 1068; continue;
  }
    case 1068: {
    /* // Preserve the receiver for a method call. */
    v31 = v32.getStillDamage(Number.call(undefined, v6), v25, arg1);
    if ([v31][0]) { pc = 1111; continue; }
    pc = 1103; continue;
  }
    case 1103: {
    v20 = [v21][0];
    pc = 1111; continue;
  }
    case 1111: {
    pc = 1112; continue;
  }
    case 1112: {
    v21 = numeric(v21) + (typeof numeric(v21) === 'bigint' ? 1n : 1);
    pc = 602; continue;
  }
    case 1123: {
    /* // Preserve the receiver for a method call. */
    v4 = [Math.max(v4, v20)][0];
    pc = 1148; continue;
  }
    case 1148: {
    const [t5, t6] = inStep(0);
    if (!t6) { pc = 86; continue; }
    pc = 1154; continue;
  }
    case 1154: {
    pc = 1155; continue;
  }
    case 1155: {
    return {damage: v5, earliestTickEscaped: v4};
  }
    }
  }
}

/** То же, но с ограничением по времени/дистанции. */
function getPathDamageBounded(arg0, arg1, arg2, {configuration, dodgePlanner, mathBindings}) {
  let v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v2, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v3, v30, v31, v32, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v32 = this;
    /* // Preserve the receiver for a method call. */
    config = configuration.getConfig("AutoDodge");
    v1 = arg1 + config["Safety"] * 10;
    v2 = 0;
    v3 = [];
    v4 = 0;
    v5 = 0;
    pc = 71; continue;
  }
    case 71: {
    pc = 1194; continue;
  }
    case 86: {
    v6 = inIterator(BattleCache["Projectiles"]);
    pc = 90; continue;
  }
    case 90: {
    if (!arg2) { pc = 163; continue; }
    pc = 120; continue;
  }
    case 120: {
    if ([v5 > arg2["damage"]][0]) { pc = 159; continue; }
    pc = 134; continue;
  }
    case 134: {
    if (!v5 === arg2["damage"]) { pc = 163; continue; }
    pc = 148; continue;
  }
    case 148: {
    pc = 159; continue;
  }
    case 159: {
    if (!v4 >= arg2["earliestTickEscaped"]) { pc = 163; continue; }
    pc = 161; continue;
  }
    case 161: {
    return null;
  }
    case 163: {
    v7 = BattleCache["Projectiles"][v6];
    if (!!![]) { pc = 201; continue; }
    pc = 187; continue;
  }
    case 187: {
    if (!v7["isIndirect"]) { pc = 201; continue; }
    pc = 198; continue;
  }
    case 198: {
    pc = 1194; continue;
  }
    case 201: {
    if ([!v7["projectileTicks"]][0]) { pc = 278; continue; }
    pc = 214; continue;
  }
    case 214: {
    if ([!v7["dodgeAllowed"]][0]) { pc = 278; continue; }
    pc = 228; continue;
  }
    case 228: {
    if ([v7["isOwnTeam"]][0]) { pc = 278; continue; }
    pc = 241; continue;
  }
    case 241: {
    if ([v7["state"] != 0][0]) { pc = 278; continue; }
    pc = 256; continue;
  }
    case 256: {
    if (!v7["isIndirect"]) { pc = 283; continue; }
    pc = 268; continue;
  }
    case 268: {
    pc = 278; continue;
  }
    case 278: {
    if (!!v7["areaEffectRadius"]) { pc = 283; continue; }
    pc = 280; continue;
  }
    case 280: {
    pc = 1194; continue;
  }
    case 283: {
    /* // Preserve the receiver for a method call. */
    if (!v3.includes(Number.call(undefined, v6))) { pc = 309; continue; }
    pc = 306; continue;
  }
    case 306: {
    pc = 1194; continue;
  }
    case 309: {
    v8 = BattleCache["CurrentTick"] >= v7["allowedTick"];
    if (!!v8) { pc = 341; continue; }
    pc = 338; continue;
  }
    case 338: {
    pc = 1194; continue;
  }
    case 341: {
    v9 = v7["projectileTicks"];
    /* // Preserve the receiver for a method call. */
    v10 = Object.keys(v9);
    v11 = Number.call(undefined, v10[v10["length"] - 1]);
    v12 = BattleCache["CurrentTick"] - v7["startTick"];
    /* // Preserve the receiver for a method call. */
    v13 = v32.getStillDamage(Number.call(undefined, v6), arg0[0], arg1);
    if (!v13) { pc = 451; continue; }
    pc = 448; continue;
  }
    case 448: {
    pc = 454; continue;
  }
    case 451: {
    pc = 454; continue;
  }
    case 454: {
    v14 = v1;
    if (!v7["ignoreExtraRadius"]) { pc = 470; continue; }
    pc = 467; continue;
  }
    case 467: {
    pc = 479; continue;
  }
    case 470: {
    pc = 479; continue;
  }
    case 479: {
    v15 = dodgePlanner["EXTRA_RADIUS"];
    if (!v7["isIndirect"]) { pc = 634; continue; }
    pc = 495; continue;
  }
    case 495: {
    v16 = v9[v11];
    v17 = v11 - v12;
    v18 = arg0[v17];
    if ([!v18][0]) { pc = 544; continue; }
    pc = 539; continue;
  }
    case 539: {
    pc = 544; continue;
  }
    case 544: {
    if (!!v16) { pc = 549; continue; }
    pc = 546; continue;
  }
    case 546: {
    pc = 1194; continue;
  }
    case 549: {
    /* // Preserve the receiver for a method call. */
    v19 = mathBindings["MathUtil"].getDistance(v18[0], v18[1], v16[0], v16[1]);
    if (!v19 <= v14 + v7["areaEffectRadius"] + v15) { pc = 1194; continue; }
    pc = 613; continue;
  }
    case 613: {
    v5 = [v5 + v7["damage"]][0];
    pc = 1201; continue;
  }
    case 634: {
    v20 = 1 / 0;
    v21 = 0;
    pc = 648; continue;
  }
    case 648: {
    if (!v21 < arg0["length"]) { pc = 1169; continue; }
    pc = 664; continue;
  }
    case 664: {
    pc = 666; continue;
  }
    case 666: {
    v22 = v21 + v2;
    v23 = v9[v22];
    if (!!v23) { pc = 708; continue; }
    pc = 705; continue;
  }
    case 705: {
    pc = 1158; continue;
  }
    case 708: {
    if (!v22 > 0) { pc = 740; continue; }
    pc = 715; continue;
  }
    case 715: {
    if (![v9[v22 - 1]][0] === undefined || [v9[v22 - 1]][0] === null) { pc = 749; continue; }
    pc = 728; continue;
  }
    case 728: {
    pc = 749; continue;
  }
    case 740: {
    pc = 749; continue;
  }
    case 749: {
    v24 = v7["startPosition"];
    v25 = arg0[v21];
    if (!v21 > 0) { pc = 774; continue; }
    pc = 765; continue;
  }
    case 765: {
    pc = 800; continue;
  }
    case 774: {
    if ([BattleCache["TickPosition"]][0]) { pc = 800; continue; }
    pc = 788; continue;
  }
    case 788: {
    pc = 800; continue;
  }
    case 800: {
    v26 = BattleCache["ServerPosition"];
    /* // Preserve the receiver for a method call. */
    if (!v32.checkSweptCollision(v24, v23, v26, v25, v14 + v7["radius"] + v15)) { pc = 941; continue; }
    pc = 844; continue;
  }
    case 844: {
    v5 = [v5 + v7["damage"]][0];
    if (!v7["damageOnlyWithOneProj"]) { pc = 1169; continue; }
    pc = 876; continue;
  }
    case 876: {
    /* // Preserve the receiver for a method call. */
    v27 = v32.getGroupedProjectiles(Number.call(undefined, v6));
    pc = 906; continue;
  }
    case 906: {
    pc = 932; continue;
  }
    case 912: {
    v28 = ofIterator(v27);
    pc = 916; continue;
  }
    case 916: {
    /* // Preserve the receiver for a method call. */
    pc = 932; continue;
  }
    case 932: {
    const [t1, t2] = ofStep(arg0[v21 - 1]);
    if (!t2) { pc = 912; continue; }
    pc = 936; continue;
  }
    case 936: {
    pc = 1169; continue;
  }
    case 941: {
    if (!v22 == v11) { pc = 1105; continue; }
    pc = 953; continue;
  }
    case 953: {
    if (!v7["areaEffectRadius"]) { pc = 1105; continue; }
    pc = 967; continue;
  }
    case 967: {
    /* // Preserve the receiver for a method call. */
    if (!v32.checkSweptCollision(v24, v23, v26, v25, v14 + v7["areaEffectRadius"] + v15)) { pc = 1105; continue; }
    pc = 1009; continue;
  }
    case 1009: {
    v5 = [v5 + v7["damage"]][0];
    if (!v7["damageOnlyWithOneProj"]) { pc = 1169; continue; }
    pc = 1041; continue;
  }
    case 1041: {
    /* // Preserve the receiver for a method call. */
    v29 = v32.getGroupedProjectiles(Number.call(undefined, v6));
    pc = 1071; continue;
  }
    case 1071: {
    pc = 1097; continue;
  }
    case 1077: {
    v30 = ofIterator(v29);
    pc = 1081; continue;
  }
    case 1081: {
    /* // Preserve the receiver for a method call. */
    pc = 1097; continue;
  }
    case 1097: {
    const [t3, t4] = ofStep(v7["startPosition"]);
    if (!t4) { pc = 1077; continue; }
    pc = 1101; continue;
  }
    case 1101: {
    pc = 1169; continue;
  }
    case 1105: {
    if (!v20 == 1 / 0) { pc = 1158; continue; }
    pc = 1114; continue;
  }
    case 1114: {
    /* // Preserve the receiver for a method call. */
    v31 = v32.getStillDamage(Number.call(undefined, v6), v25, arg1);
    if ([v31][0]) { pc = 1157; continue; }
    pc = 1149; continue;
  }
    case 1149: {
    v20 = [v21][0];
    pc = 1157; continue;
  }
    case 1157: {
    pc = 1158; continue;
  }
    case 1158: {
    v21 = numeric(v21) + (typeof numeric(v21) === 'bigint' ? 1n : 1);
    pc = 648; continue;
  }
    case 1169: {
    /* // Preserve the receiver for a method call. */
    v4 = [Math.max(v4, v20)][0];
    pc = 1194; continue;
  }
    case 1194: {
    const [t5, t6] = inStep(0);
    if (!t6) { pc = 86; continue; }
    pc = 1200; continue;
  }
    case 1200: {
    pc = 1201; continue;
  }
    case 1201: {
    return {damage: v5, earliestTickEscaped: v4};
  }
    }
  }
}

/** Урон, если стоять на месте. */
function getStillDamage(arg0, arg1, arg2, {dodgePlanner, mathBindings}) {
  let arg1, v0, v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v3, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v29 = this;
    v0 = arg1[0];
    v1 = arg1[1];
    if ([typeof v0 !== "number"][0]) { pc = 89; continue; }
    pc = 57; continue;
  }
    case 57: {
    if ([typeof v1 !== "number"][0]) { pc = 89; continue; }
    pc = 71; continue;
  }
    case 71: {
    pc = 89; continue;
  }
    case 89: {
    if (!v0 - v0 + v1 - v1 !== 0) { pc = 105; continue; }
    pc = 91; continue;
  }
    case 91: {
    /* // Preserve the receiver for a method call. */
    /* return v29.getStillDamageOriginal(arg0, arg1, arg2) */
  }
    case 105: {
    arg1 = arg1;
    v3 = [];
    v4 = 0;
    v5 = BattleCache["Projectiles"][arg0];
    if (!!![]) { pc = 148; continue; }
    pc = 136; continue;
  }
    case 136: {
    if (!v5["isIndirect"]) { pc = 148; continue; }
    pc = 147; continue;
  }
    case 147: {
    return;
  }
    case 148: {
    if ([!v5["projectileTicks"]][0]) { pc = 225; continue; }
    pc = 161; continue;
  }
    case 161: {
    if ([!v5["dodgeAllowed"]][0]) { pc = 225; continue; }
    pc = 175; continue;
  }
    case 175: {
    if ([v5["isOwnTeam"]][0]) { pc = 225; continue; }
    pc = 188; continue;
  }
    case 188: {
    if ([v5["state"] != 0][0]) { pc = 225; continue; }
    pc = 203; continue;
  }
    case 203: {
    if (!v5["isIndirect"]) { pc = 228; continue; }
    pc = 215; continue;
  }
    case 215: {
    pc = 225; continue;
  }
    case 225: {
    if (!!v5["areaEffectRadius"]) { pc = 228; continue; }
    pc = 227; continue;
  }
    case 227: {
    return;
  }
    case 228: {
    /* // Preserve the receiver for a method call. */
    if (!v3.includes(Number.call(undefined, arg0))) { pc = 250; continue; }
    pc = 249; continue;
  }
    case 249: {
    return;
  }
    case 250: {
    v6 = BattleCache["CurrentTick"] >= v5["allowedTick"];
    if (!!v6) { pc = 280; continue; }
    pc = 279; continue;
  }
    case 279: {
    return;
  }
    case 280: {
    v7 = v5["projectileTicks"];
    /* // Preserve the receiver for a method call. */
    v8 = Object.keys(v7);
    v9 = Number.call(undefined, v8[v8["length"] - 1]);
    if (!v5["ignoreExtraRadius"]) { pc = 347; continue; }
    pc = 344; continue;
  }
    case 344: {
    pc = 356; continue;
  }
    case 347: {
    pc = 356; continue;
  }
    case 356: {
    v10 = dodgePlanner["EXTRA_RADIUS"];
    pc = 363; continue;
  }
    case 363: {
    pc = 1014; continue;
  }
    case 370: {
    v11 = inIterator(v7);
    pc = 374; continue;
  }
    case 374: {
    v12 = v7[v11];
    v13 = v7[Number.call(undefined, v11)];
    v14 = v12[0];
    v15 = v12[1];
    if (![v13 === v12][0]) { pc = 495; continue; }
    pc = 449; continue;
  }
    case 449: {
    if (![typeof v14 === "number"][0]) { pc = 495; continue; }
    pc = 463; continue;
  }
    case 463: {
    if (![typeof v15 === "number"][0]) { pc = 495; continue; }
    pc = 477; continue;
  }
    case 477: {
    pc = 495; continue;
  }
    case 495: {
    v16 = v14 - v14 + v15 - v15 === 0;
    v17 = v14 - v0;
    v18 = v15 - v1;
    v19 = v17 * v17 + v18 * v18;
    v20 = arg2 + v5["radius"] + v10;
    if (!v5["isIndirect"]) { pc = 680; continue; }
    pc = 560; continue;
  }
    case 560: {
    if (!Number.call(undefined, v11) == v9) { pc = 1014; continue; }
    pc = 578; continue;
  }
    case 578: {
    v21 = v12;
    v22 = arg1;
    /* // Preserve the receiver for a method call. */
    v23 = mathBindings["MathUtil"].getDistance(v22[0], v22[1], v21[0], v21[1]);
    if (!v23 <= arg2 + v5["areaEffectRadius"] + v10) { pc = 1014; continue; }
    pc = 659; continue;
  }
    case 659: {
    v4 = [v4 + v5["damage"]][0];
    pc = 1021; continue;
  }
    case 680: {
    if (!v16) { pc = 700; continue; }
    pc = 685; continue;
  }
    case 685: {
    pc = 726; continue;
  }
    case 700: {
    /* // Preserve the receiver for a method call. */
    pc = 726; continue;
  }
    case 726: {
    if (!v29.checkSweptCollision(v13, v12, arg1, arg1, v20)) { pc = 823; continue; }
    pc = 728; continue;
  }
    case 728: {
    v4 = [v4 + v5["damage"]][0];
    if (!v5["damageOnlyWithOneProj"]) { pc = 1021; continue; }
    pc = 760; continue;
  }
    case 760: {
    /* // Preserve the receiver for a method call. */
    v24 = v29.getGroupedProjectiles(Number.call(undefined, arg0));
    pc = 788; continue;
  }
    case 788: {
    pc = 814; continue;
  }
    case 794: {
    v25 = ofIterator(v24);
    pc = 798; continue;
  }
    case 798: {
    /* // Preserve the receiver for a method call. */
    pc = 814; continue;
  }
    case 814: {
    const [t1, t2] = ofStep(v19 - v20 * v20 <= 0);
    if (!t2) { pc = 794; continue; }
    pc = 818; continue;
  }
    case 818: {
    pc = 1021; continue;
  }
    case 823: {
    if (!Number.call(undefined, v11) == v9) { pc = 1014; continue; }
    pc = 841; continue;
  }
    case 841: {
    if (!v5["areaEffectRadius"]) { pc = 1014; continue; }
    pc = 855; continue;
  }
    case 855: {
    v26 = arg2 + v5["areaEffectRadius"] + v10;
    if (!v16) { pc = 895; continue; }
    pc = 880; continue;
  }
    case 880: {
    pc = 921; continue;
  }
    case 895: {
    /* // Preserve the receiver for a method call. */
    pc = 921; continue;
  }
    case 921: {
    if (!v29.checkSweptCollision(v13, v12, arg1, arg1, v26)) { pc = 1014; continue; }
    pc = 923; continue;
  }
    case 923: {
    v4 = [v4 + v5["damage"]][0];
    if (!v5["damageOnlyWithOneProj"]) { pc = 1021; continue; }
    pc = 952; continue;
  }
    case 952: {
    /* // Preserve the receiver for a method call. */
    v27 = v29.getGroupedProjectiles(Number.call(undefined, arg0));
    pc = 980; continue;
  }
    case 980: {
    pc = 1006; continue;
  }
    case 986: {
    v28 = ofIterator(v27);
    pc = 990; continue;
  }
    case 990: {
    /* // Preserve the receiver for a method call. */
    pc = 1006; continue;
  }
    case 1006: {
    const [t3, t4] = ofStep(v19 - v26 * v26 <= 0);
    if (!t4) { pc = 986; continue; }
    pc = 1010; continue;
  }
    case 1010: {
    pc = 1021; continue;
  }
    case 1014: {
    const [t5, t6] = inStep(0);
    if (!t6) { pc = 370; continue; }
    pc = 1020; continue;
  }
    case 1020: {
    pc = 1021; continue;
  }
    case 1021: {
    return v4;
  }
    }
  }
}

/** Оригинальная (без поправок) оценка урона на месте. */
function getStillDamageOriginal(arg0, arg1, arg2, {configuration, dodgePlanner, mathBindings}) {
  let arg1, v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v2, v3, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v19 = this;
    /* // Preserve the receiver for a method call. */
    arg1 = arg1;
    v1 = [];
    v2 = 0;
    v3 = BattleCache["Projectiles"][arg0];
    if (!!![]) { pc = 89; continue; }
    pc = 77; continue;
  }
    case 77: {
    if (!v3["isIndirect"]) { pc = 89; continue; }
    pc = 88; continue;
  }
    case 88: {
    return;
  }
    case 89: {
    if ([!v3["projectileTicks"]][0]) { pc = 166; continue; }
    pc = 102; continue;
  }
    case 102: {
    if ([!v3["dodgeAllowed"]][0]) { pc = 166; continue; }
    pc = 116; continue;
  }
    case 116: {
    if ([v3["isOwnTeam"]][0]) { pc = 166; continue; }
    pc = 129; continue;
  }
    case 129: {
    if ([v3["state"] != 0][0]) { pc = 166; continue; }
    pc = 144; continue;
  }
    case 144: {
    if (!v3["isIndirect"]) { pc = 169; continue; }
    pc = 156; continue;
  }
    case 156: {
    pc = 166; continue;
  }
    case 166: {
    if (!!v3["areaEffectRadius"]) { pc = 169; continue; }
    pc = 168; continue;
  }
    case 168: {
    return;
  }
    case 169: {
    /* // Preserve the receiver for a method call. */
    if (!v1.includes(Number.call(undefined, arg0))) { pc = 191; continue; }
    pc = 190; continue;
  }
    case 190: {
    return;
  }
    case 191: {
    v4 = BattleCache["CurrentTick"] >= v3["allowedTick"];
    if (!!v4) { pc = 221; continue; }
    pc = 220; continue;
  }
    case 220: {
    return;
  }
    case 221: {
    v5 = v3["projectileTicks"];
    /* // Preserve the receiver for a method call. */
    v6 = Object.keys(v5);
    v7 = Number.call(undefined, v6[v6["length"] - 1]);
    if (!v3["ignoreExtraRadius"]) { pc = 288; continue; }
    pc = 285; continue;
  }
    case 285: {
    pc = 297; continue;
  }
    case 288: {
    pc = 297; continue;
  }
    case 297: {
    v8 = dodgePlanner["EXTRA_RADIUS"];
    pc = 304; continue;
  }
    case 304: {
    pc = 774; continue;
  }
    case 311: {
    v9 = inIterator(v5);
    pc = 315; continue;
  }
    case 315: {
    v10 = v5[v9];
    v11 = v5[Number.call(undefined, v9)];
    if (!v3["isIndirect"]) { pc = 476; continue; }
    pc = 356; continue;
  }
    case 356: {
    if (!Number.call(undefined, v9) == v7) { pc = 774; continue; }
    pc = 374; continue;
  }
    case 374: {
    v12 = v10;
    v13 = arg1;
    /* // Preserve the receiver for a method call. */
    v14 = mathBindings["MathUtil"].getDistance(v13[0], v13[1], v12[0], v12[1]);
    if (!v14 <= arg2 + v3["areaEffectRadius"] + v8) { pc = 774; continue; }
    pc = 455; continue;
  }
    case 455: {
    v2 = [v2 + v3["damage"]][0];
    pc = 781; continue;
  }
    case 476: {
    /* // Preserve the receiver for a method call. */
    if (!v19.checkSweptCollision(v11, v10, arg1, arg1, arg2 + v3["radius"] + v8)) { pc = 611; continue; }
    pc = 516; continue;
  }
    case 516: {
    v2 = [v2 + v3["damage"]][0];
    if (!v3["damageOnlyWithOneProj"]) { pc = 781; continue; }
    pc = 548; continue;
  }
    case 548: {
    /* // Preserve the receiver for a method call. */
    v15 = v19.getGroupedProjectiles(Number.call(undefined, arg0));
    pc = 576; continue;
  }
    case 576: {
    pc = 602; continue;
  }
    case 582: {
    v16 = ofIterator(v15);
    pc = 586; continue;
  }
    case 586: {
    /* // Preserve the receiver for a method call. */
    pc = 602; continue;
  }
    case 602: {
    const [t1, t2] = ofStep(0);
    if (!t2) { pc = 582; continue; }
    pc = 606; continue;
  }
    case 606: {
    pc = 781; continue;
  }
    case 611: {
    if (!Number.call(undefined, v9) == v7) { pc = 774; continue; }
    pc = 629; continue;
  }
    case 629: {
    if (!v3["areaEffectRadius"]) { pc = 774; continue; }
    pc = 643; continue;
  }
    case 643: {
    /* // Preserve the receiver for a method call. */
    if (!v19.checkSweptCollision(v11, v10, arg1, arg1, arg2 + v3["areaEffectRadius"] + v8)) { pc = 774; continue; }
    pc = 683; continue;
  }
    case 683: {
    v2 = [v2 + v3["damage"]][0];
    if (!v3["damageOnlyWithOneProj"]) { pc = 781; continue; }
    pc = 712; continue;
  }
    case 712: {
    /* // Preserve the receiver for a method call. */
    v17 = v19.getGroupedProjectiles(Number.call(undefined, arg0));
    pc = 740; continue;
  }
    case 740: {
    pc = 766; continue;
  }
    case 746: {
    v18 = ofIterator(v17);
    pc = 750; continue;
  }
    case 750: {
    /* // Preserve the receiver for a method call. */
    pc = 766; continue;
  }
    case 766: {
    const [t3, t4] = ofStep(/*iterator*/);
    if (!t4) { pc = 746; continue; }
    pc = 770; continue;
  }
    case 770: {
    pc = 781; continue;
  }
    case 774: {
    const [t5, t6] = inStep(/*iterator*/);
    if (!t6) { pc = 311; continue; }
    pc = 780; continue;
  }
    case 780: {
    pc = 781; continue;
  }
    case 781: {
    return v2;
  }
    }
  }
}

/** Урон по конкретной позиции с учётом всех снарядов. */
function getDamage(arg0, {configuration, dodgePlanner, mathBindings}) {
  let v0, v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v2, v20, v3, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v20 = this;
    /* // Preserve the receiver for a method call. */
    if ([BattleCache["TickPosition"]][0]) { pc = 56; continue; }
    pc = 44; continue;
  }
    case 44: {
    pc = 56; continue;
  }
    case 56: {
    v0 = BattleCache["ServerPosition"];
    v1 = [];
    v2 = 0;
    pc = 68; continue;
  }
    case 68: {
    pc = 847; continue;
  }
    case 83: {
    v3 = inIterator(BattleCache["Projectiles"]);
    pc = 86; continue;
  }
    case 86: {
    v4 = BattleCache["Projectiles"][v3];
    if (!!![]) { pc = 142; continue; }
    pc = 128; continue;
  }
    case 128: {
    if (!v4["isIndirect"]) { pc = 142; continue; }
    pc = 139; continue;
  }
    case 139: {
    pc = 847; continue;
  }
    case 142: {
    if ([!v4["projectileTicks"]][0]) { pc = 219; continue; }
    pc = 155; continue;
  }
    case 155: {
    if ([!v4["dodgeAllowed"]][0]) { pc = 219; continue; }
    pc = 169; continue;
  }
    case 169: {
    if ([v4["isOwnTeam"]][0]) { pc = 219; continue; }
    pc = 182; continue;
  }
    case 182: {
    if ([v4["state"] != 0][0]) { pc = 219; continue; }
    pc = 197; continue;
  }
    case 197: {
    if (!v4["isIndirect"]) { pc = 224; continue; }
    pc = 209; continue;
  }
    case 209: {
    pc = 219; continue;
  }
    case 219: {
    if (!!v4["areaEffectRadius"]) { pc = 224; continue; }
    pc = 221; continue;
  }
    case 221: {
    pc = 847; continue;
  }
    case 224: {
    /* // Preserve the receiver for a method call. */
    if (!v1.includes(Number.call(undefined, v3))) { pc = 250; continue; }
    pc = 247; continue;
  }
    case 247: {
    pc = 847; continue;
  }
    case 250: {
    v5 = BattleCache["CurrentTick"] >= v4["allowedTick"];
    if (!!v5) { pc = 282; continue; }
    pc = 279; continue;
  }
    case 279: {
    pc = 847; continue;
  }
    case 282: {
    v6 = v4["projectileTicks"];
    /* // Preserve the receiver for a method call. */
    v7 = Object.keys(v6);
    v8 = Number.call(undefined, v7[v7["length"] - 1]);
    if (!v4["ignoreExtraRadius"]) { pc = 349; continue; }
    pc = 346; continue;
  }
    case 346: {
    pc = 358; continue;
  }
    case 349: {
    pc = 358; continue;
  }
    case 358: {
    v9 = dodgePlanner["EXTRA_RADIUS"];
    pc = 365; continue;
  }
    case 365: {
    pc = 839; continue;
  }
    case 372: {
    v10 = inIterator(v6);
    pc = 376; continue;
  }
    case 376: {
    v11 = v6[v10];
    v12 = v6[Number.call(undefined, v10)];
    if (!v4["isIndirect"]) { pc = 537; continue; }
    pc = 417; continue;
  }
    case 417: {
    if (!Number.call(undefined, v10) == v8) { pc = 839; continue; }
    pc = 435; continue;
  }
    case 435: {
    v13 = v11;
    v14 = v0;
    /* // Preserve the receiver for a method call. */
    v15 = mathBindings["MathUtil"].getDistance(v14[0], v14[1], v13[0], v13[1]);
    if (!v15 <= arg0 + v4["areaEffectRadius"] + v9) { pc = 839; continue; }
    pc = 516; continue;
  }
    case 516: {
    v2 = [v2 + v4["damage"]][0];
    pc = 846; continue;
  }
    case 537: {
    /* // Preserve the receiver for a method call. */
    if (!v20.checkSweptCollision(v12, v11, v0, v0, arg0 + v4["radius"] + v9)) { pc = 674; continue; }
    pc = 577; continue;
  }
    case 577: {
    v2 = [v2 + v4["damage"]][0];
    if (!v4["damageOnlyWithOneProj"]) { pc = 846; continue; }
    pc = 609; continue;
  }
    case 609: {
    /* // Preserve the receiver for a method call. */
    v16 = v20.getGroupedProjectiles(Number.call(undefined, v3));
    pc = 639; continue;
  }
    case 639: {
    pc = 665; continue;
  }
    case 645: {
    v17 = ofIterator(v16);
    pc = 649; continue;
  }
    case 649: {
    /* // Preserve the receiver for a method call. */
    pc = 665; continue;
  }
    case 665: {
    const [t1, t2] = ofStep(0);
    if (!t2) { pc = 645; continue; }
    pc = 669; continue;
  }
    case 669: {
    pc = 846; continue;
  }
    case 674: {
    if (!Number.call(undefined, v10) == v8) { pc = 839; continue; }
    pc = 692; continue;
  }
    case 692: {
    if (!v4["areaEffectRadius"]) { pc = 839; continue; }
    pc = 706; continue;
  }
    case 706: {
    /* // Preserve the receiver for a method call. */
    if (!v20.checkSweptCollision(v12, v11, v0, v0, arg0 + v4["areaEffectRadius"] + v9)) { pc = 839; continue; }
    pc = 746; continue;
  }
    case 746: {
    v2 = [v2 + v4["damage"]][0];
    if (!v4["damageOnlyWithOneProj"]) { pc = 846; continue; }
    pc = 775; continue;
  }
    case 775: {
    /* // Preserve the receiver for a method call. */
    v18 = v20.getGroupedProjectiles(Number.call(undefined, v3));
    pc = 805; continue;
  }
    case 805: {
    pc = 831; continue;
  }
    case 811: {
    v19 = ofIterator(v18);
    pc = 815; continue;
  }
    case 815: {
    /* // Preserve the receiver for a method call. */
    pc = 831; continue;
  }
    case 831: {
    const [t3, t4] = ofStep(/*iterator*/);
    if (!t4) { pc = 811; continue; }
    pc = 835; continue;
  }
    case 835: {
    pc = 846; continue;
  }
    case 839: {
    const [t5, t6] = inStep(/*iterator*/);
    if (!t6) { pc = 372; continue; }
    pc = 845; continue;
  }
    case 845: {
    pc = 846; continue;
  }
    case 846: {
    pc = 847; continue;
  }
    case 847: {
    const [t7, t8] = inStep(/*iterator*/);
    if (!t8) { pc = 83; continue; }
    pc = 853; continue;
  }
    case 853: {
    return v2;
  }
    }
  }
}

/** Позиция вращающегося/бумерангового снаряда на тике. */
function getProjectilePathPoint({capturedValue0, capturedValue1, capturedValue2, capturedValue3}) {
  let v0, v1;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    if (!!capturedValue0["projectileTicks"]) { pc = 28; continue; }
    pc = 15; continue;
  }
    case 15: {
    if (!!capturedValue0["isDoNotRotateClip"]) { pc = 28; continue; }
    pc = 27; continue;
  }
    case 27: {
    return;
  }
    case 28: {
    if (!capturedValue0["rotates"]) { pc = 40; continue; }
    pc = 39; continue;
  }
    case 39: {
    return;
  }
    case 40: {
    v0 = BattleCache["CurrentTick"] - capturedValue0["startTick"];
    if (!v0 <= 1) { pc = 82; continue; }
    pc = 69; continue;
  }
    case 69: {
    if (!!capturedValue0["isDoNotRotateClip"]) { pc = 82; continue; }
    pc = 81; continue;
  }
    case 81: {
    return;
  }
    case 82: {
    if ([capturedValue0["startPosition"][0] != capturedValue1[0]][0]) { pc = 120; continue; }
    pc = 102; continue;
  }
    case 102: {
    pc = 120; continue;
  }
    case 120: {
    if (!capturedValue0["startPosition"][1] != capturedValue1[1]) { pc = 330; continue; }
    pc = 125; continue;
  }
    case 125: {
    /* // Preserve the receiver for a method call. */
    /* capturedValue0[propertyKey("startAngle")] = capturedValue2["MathUtil"].getDegreesToPoint(capturedValue0["startPosition"][0], capturedValue0["startPosition"][1], capturedValue1[0], capturedValue1[1]) */
    /* capturedValue0[propertyKey("setFirstAngle")] = !![] */
    if (!!![]) { pc = 244; continue; }
    pc = 208; continue;
  }
    case 208: {
    if ([capturedValue0["dodgeAllowed"] === ![]][0]) { pc = 241; continue; }
    pc = 225; continue;
  }
    case 225: {
    pc = 241; continue;
  }
    case 241: {
    if (!capturedValue0["isOwnTeam"] === !![]) { pc = 244; continue; }
    pc = 243; continue;
  }
    case 243: {
    return;
  }
    case 244: {
    /* // Preserve the receiver for a method call. */
    if (![!capturedValue0["isIndirect"]][0]) { pc = 313; continue; }
    pc = 302; continue;
  }
    case 302: {
    pc = 313; continue;
  }
    case 313: {
    v1 = capturedValue3.deferProjectileTicks(capturedValue0["range"], capturedValue0["speed"], capturedValue0["position"], capturedValue0["startAngle"], !capturedValue0["passesEnvironment"]);
    /* capturedValue0[propertyKey("projectileTicks")] = v1 */
    pc = 330; continue;
  }
    case 330: {
    return;
  }
    }
  }
}

/** Возврат тача в исходную точку (release dodge). */
function releaseDodgeTouch({capturedValue0, capturedValue1, capturedValue2}) {
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    if (![capturedValue0["/*/*/*capturedValue0["/*/*/*capturedValue0["/*/*/*capturedValue0["/*/*/*x*/*/*/"]*/*/*/"]*/*/*/"]*/*/*/"] == capturedValue0["originalX"]][0]) { pc = 42; continue; }
    pc = 22; continue;
  }
    case 22: {
    pc = 42; continue;
  }
    case 42: {
    if ([capturedValue0["capturedValue0["originalX"]"] == capturedValue0["originalY"]][0]) { pc = 79; continue; }
    pc = 45; continue;
  }
    case 45: {
    /* // Preserve the receiver for a method call. */
    pc = 79; continue;
  }
    case 79: {
    /* BattleCache[propertyKey("GoingBackToOriginalDodge")] = ![] */
    if (!capturedValue0["active"]) { pc = 124; continue; }
    pc = 107; continue;
  }
    case 107: {
    /* // Preserve the receiver for a method call. */
    pc = 139; continue;
  }
    case 124: {
    /* // Preserve the receiver for a method call. */
    pc = 139; continue;
  }
    case 139: {
    return;
  }
    }
  }
}

/** Тики полёта снаряда по дистанции. */
function getProjectileTicks(arg0, arg1, arg2, arg3, arg4, {mathBindings, dodgePlanner}) {
  let arg2, arg3, radians, v0, v10, v11, v12, v13, v3, v4, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v0 = {0: arg2};
    arg2 = arg2;
    arg3 = arg3;
    v3 = arg1 / 20;
    /* // Preserve the receiver for a method call. */
    v4 = Math.ceil(arg0 / v3);
    /* // Preserve the receiver for a method call. */
    radians = mathBindings["MathUtil"].getRadians(arg3);
    /* // Preserve the receiver for a method call. */
    v6 = Math.cos(radians);
    /* // Preserve the receiver for a method call. */
    v7 = Math.sin(radians);
    v8 = 1;
    pc = 127; continue;
  }
    case 127: {
    if (!v8 <= v4) { pc = 300; continue; }
    pc = 139; continue;
  }
    case 139: {
    pc = 141; continue;
  }
    case 141: {
    /* // Preserve the receiver for a method call. */
    v9 = Math.min(v3, arg0 - v8 - 1 * v3);
    v10 = arg2[0] + v6 * v9;
    v11 = arg2[1] + v7 * v9;
    v12 = [v10, v11];
    if (!arg4) { pc = 270; continue; }
    pc = 229; continue;
  }
    case 229: {
    /* // Preserve the receiver for a method call. */
    v13 = dodgePlanner.checkWallCollisions(arg2, v12);
    if (!v13) { pc = 270; continue; }
    pc = 257; continue;
  }
    case 257: {
    /* v0[propertyKey(v8)] = v13 */
    pc = 300; continue;
  }
    case 270: {
    /* v0[propertyKey(v8)] = v12 */
    arg2 = [v12][0];
    v8 = numeric(v8) + (typeof numeric(v8) === 'bigint' ? 1n : 1);
    pc = 127; continue;
  }
    case 300: {
    return v0;
  }
    }
  }
}

/** Позиция снаряда на заданном тике. */
function getProjectilePosition(arg0, {capturedValue0, capturedValue1, capturedValue2, capturedValue3}) {
  let v0, v1, v2, v3, v4;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v4 = this;
    v0 = capturedValue0["LogicGameObject"];
    if (!v4["getX"] === capturedValue1) { pc = 382; continue; }
    pc = 33; continue;
  }
    case 33: {
    if (!v4["getY"] === capturedValue2) { pc = 382; continue; }
    pc = 50; continue;
  }
    case 50: {
    if ([!arg0][0]) { pc = 68; continue; }
    pc = 55; continue;
  }
    case 55: {
    pc = 68; continue;
  }
    case 68: {
    if (!v4["getZ"] === capturedValue3) { pc = 382; continue; }
    pc = 73; continue;
  }
    case 73: {
    /* // Preserve the receiver for a method call. */
    if (!Number.isSafeInteger(v0["X"])) { pc = 382; continue; }
    pc = 101; continue;
  }
    case 101: {
    if (!v0["X"] >= 0) { pc = 382; continue; }
    pc = 117; continue;
  }
    case 117: {
    if (!v0["Y"] === v0["X"] + 4) { pc = 382; continue; }
    pc = 143; continue;
  }
    case 143: {
    if ([!arg0][0]) { pc = 171; continue; }
    pc = 148; continue;
  }
    case 148: {
    pc = 171; continue;
  }
    case 171: {
    if (!v0["Z"] === v0["X"] + 8) { pc = 382; continue; }
    pc = 176; continue;
  }
    case 176: {
    if (!arg0) { pc = 192; continue; }
    pc = 188; continue;
  }
    case 188: {
    pc = 194; continue;
  }
    case 192: {
    pc = 194; continue;
  }
    case 194: {
    v1 = 8;
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    v2 = v4["instance"].add(v0["X"]).readByteArray(v1);
    if ([v2 === null][0]) { pc = 255; continue; }
    pc = 241; continue;
  }
    case 241: {
    pc = 255; continue;
  }
    case 255: {
    if (!v2["byteLength"] !== v1) { pc = 267; continue; }
    pc = 257; continue;
  }
    case 257: {
    throw throw Reflect.construct([Error][0], [], [Error][0]);
  }
    case 267: {
    v3 = Reflect.construct([DataView][0], [v2], [DataView][0]);
    if (!arg0) { pc = 342; continue; }
    pc = 283; continue;
  }
    case 283: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    return [v3.getInt32(0, !![]), v3.getInt32(4, !![]), v3.getInt32(8, !![])];
  }
    case 342: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    return [v3.getInt32(0, !![]), v3.getInt32(4, !![])];
  }
    case 382: {
    if (!arg0) { pc = 422; continue; }
    pc = 385; continue;
  }
    case 385: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    return [v4.getX(), v4.getY(), v4.getZ()];
  }
    case 422: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    return [v4.getX(), v4.getY()];
  }
    }
  }
}

/** Группировка снарядов одного выстрела. */
function getGroupedProjectiles(arg0) {
  let v0, v1, v2, v3, v4, v5, v6, v7;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v0 = BattleCache["Projectiles"][arg0];
    v1 = v0["startTick"];
    v2 = v0["index"];
    v3 = [arg0];
    /* setName(frame.makeClosure(0, {args, locals, refs, frame}), "") */
    v4 = frame.makeClosure(0, {args, locals, refs, frame});
    v5 = arg0 - 1;
    pc = 71; continue;
  }
    case 71: {
    if (!!![]) { pc = 126; continue; }
    pc = 78; continue;
  }
    case 78: {
    v6 = v4.call(undefined, v5);
    if (!!v6) { pc = 98; continue; }
    pc = 96; continue;
  }
    case 96: {
    pc = 126; continue;
  }
    case 98: {
    /* // Preserve the receiver for a method call. */
    v5 = [v5 - 1][0];
    pc = 71; continue;
  }
    case 126: {
    v5 = [arg0 + 1][0];
    pc = 134; continue;
  }
    case 134: {
    if (!!![]) { pc = 189; continue; }
    pc = 141; continue;
  }
    case 141: {
    v7 = v4.call(undefined, v5);
    if (!!v7) { pc = 161; continue; }
    pc = 159; continue;
  }
    case 159: {
    pc = 189; continue;
  }
    case 161: {
    /* // Preserve the receiver for a method call. */
    v5 = [v5 + 1][0];
    pc = 134; continue;
  }
    case 189: {
    return v3;
  }
    }
  }
}

/** Снятие подписок и очистка состояния. */
function destroyDodgeController(arg0, {configuration, captured_getDamage_getMaxRange, captured_get_getRegistry}) {
  let globalID, v0, v2, v3, v4;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    /* // Preserve the receiver for a method call. */
    if (!!configuration.isModEnabled("AutoDodge")) { pc = 30; continue; }
    pc = 29; continue;
  }
    case 29: {
    return;
  }
    case 30: {
    if (!!BattleCache["Projectiles"]) { pc = 45; continue; }
    pc = 44; continue;
  }
    case 44: {
    return;
  }
    case 45: {
    v0 = Reflect.construct([captured_getDamage_getMaxRange][0], [arg0], [captured_getDamage_getMaxRange][0]);
    /* // Preserve the receiver for a method call. */
    globalID = v0.getGlobalID();
    v2 = BattleCache["Projectiles"][globalID];
    if (!!v2) { pc = 90; continue; }
    pc = 89; continue;
  }
    case 89: {
    return;
  }
    case 90: {
    pc = 95; continue;
  }
    case 95: {
    /* // Preserve the receiver for a method call. */
    pc = 175; continue;
  }
    case 110: {
    v3 = inIterator(captured_get_getRegistry.getRegistry());
    pc = 113; continue;
  }
    case 113: {
    /* // Preserve the receiver for a method call. */
    /* // Preserve the receiver for a method call. */
    if (!!v3.includes(globalID.toString())) { pc = 145; continue; }
    pc = 143; continue;
  }
    case 143: {
    pc = 175; continue;
  }
    case 145: {
    /* // Preserve the receiver for a method call. */
    v4 = captured_get_getRegistry.get(v3);
    /* // Preserve the receiver for a method call. */
    pc = 175; continue;
  }
    case 175: {
    const [t1, t2] = inStep(/*iterator*/);
    if (!t2) { pc = 110; continue; }
    pc = 178; continue;
  }
    case 178: {
    return;
  }
    }
  }
}

/** Урон только от одного снаряда группы. */
function getDamageOnlyWithOneProj({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const DamageOnlyWithOneProj6 = LogicProjectileData5["DamageOnlyWithOneProj"];
  const callResult7 = Reflect.apply(add3, instance2, [DamageOnlyWithOneProj6]);
  const readU88 = callResult7["readU8"];
  const callResult9 = Reflect.apply(readU88, callResult7, []);
  const result10 = !callResult9;
  const result11 = !result10;
  return result11;
}

/** Максимальная дальность снаряда. */
function getMaxRange(arg0, {capturedValue0}) {
  let v0, v1, v2;
  const capturedValue01 = capturedValue0;
  const argument02 = arg0;
  const propertyValue3 = capturedValue01[argument02];
  if (propertyValue3) {
  const localValue04 = v0;
  const Range5 = localValue04["Range"];
  if (Range5) {
  const localValue16 = v1;
  const result7 = !localValue16;
  if (!result7) {
  const localValue18 = v1;
  const localValue29 = v2;
  return localValue29;
  } else {
  return undefined;
  } else {
  const GlobalCache10 = GlobalCache;
  const CastingRanges11 = GlobalCache10["CastingRanges"];
  const argument012 = arg0;
  const propertyValue13 = CastingRanges11[argument012];
  if (propertyValue13) {
  const localValue114 = v1;
  const result15 = !localValue114;
  if (!result15) {
  const localValue116 = v1;
  const localValue217 = v2;
  return localValue217;
  } else {
  return undefined;
  } else {
  const localValue018 = v0;
  const ProjectileRange19 = localValue018["ProjectileRange"];
  if (!ProjectileRange19) {
  const localValue120 = v1;
  const result21 = !localValue120;
  if (!result21) {
  const localValue122 = v1;
  const localValue223 = v2;
  return localValue223;
  } else {
  return undefined;
  } else {
  const GlobalCache24 = GlobalCache;
  const CastingRanges25 = GlobalCache24["CastingRanges"];
  const localValue026 = v0;
  const ProjectileRange27 = localValue026["ProjectileRange"];
  const propertyValue28 = CastingRanges25[ProjectileRange27];
  const localValue129 = v1;
  const result30 = !localValue129;
  if (!result30) {
  const localValue131 = v1;
  const localValue232 = v2;
  return localValue232;
  } else {
  return undefined;
  } else {
  const objectValue33 = {};
  const localValue034 = v0;
  const Range35 = localValue034["Range"];
  if (Range35) {
  const localValue136 = v1;
  const result37 = !localValue136;
  if (!result37) {
  const localValue138 = v1;
  const localValue239 = v2;
  return localValue239;
  } else {
  return undefined;
  } else {
  const GlobalCache40 = GlobalCache;
  const CastingRanges41 = GlobalCache40["CastingRanges"];
  const argument042 = arg0;
  const propertyValue43 = CastingRanges41[argument042];
  if (propertyValue43) {
  const localValue144 = v1;
  const result45 = !localValue144;
  if (!result45) {
  const localValue146 = v1;
  const localValue247 = v2;
  return localValue247;
  } else {
  return undefined;
  } else {
  const localValue048 = v0;
  const ProjectileRange49 = localValue048["ProjectileRange"];
  if (!ProjectileRange49) {
  const localValue150 = v1;
  const result51 = !localValue150;
  if (!result51) {
  const localValue152 = v1;
  const localValue253 = v2;
  return localValue253;
  } else {
  return undefined;
  } else {
  const GlobalCache54 = GlobalCache;
  const CastingRanges55 = GlobalCache54["CastingRanges"];
  const localValue056 = v0;
  const ProjectileRange57 = localValue056["ProjectileRange"];
  const propertyValue58 = CastingRanges55[ProjectileRange57];
  const localValue159 = v1;
  const result60 = !localValue159;
  if (!result60) {
  const localValue161 = v1;
  const localValue262 = v2;
  return localValue262;
  } else {
  return undefined;
}

/** Время полёта для снарядов с постоянной скоростью. */
function getConstantFlyTime({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const ConstantFlyTime6 = LogicProjectileData5["ConstantFlyTime"];
  const callResult7 = Reflect.apply(add3, instance2, [ConstantFlyTime6]);
  const readU88 = callResult7["readU8"];
  const callResult9 = Reflect.apply(readU88, callResult7, []);
  return callResult9;
}

/** Радиус коллизии снаряда. */
function getCollisionRadius({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicCharacterData5 = capturedValue04["LogicCharacterData"];
  const CollisionRadius6 = LogicCharacterData5["CollisionRadius"];
  const callResult7 = Reflect.apply(add3, instance2, [CollisionRadius6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Тип бумеранга. */
function getBoomerangType({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const BoomerangType6 = LogicProjectileData5["BoomerangType"];
  const callResult7 = Reflect.apply(add3, instance2, [BoomerangType6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Зеркальная копия угла/вектора. */
function getMirrored({capturedValue0}) {
  const base1 = base;
  const add2 = base1["add"];
  const capturedValue03 = capturedValue0;
  const Mirrored4 = capturedValue03["Mirrored"];
  const callResult5 = Reflect.apply(add2, base1, [Mirrored4]);
  const readU86 = callResult5["readU8"];
  const callResult7 = Reflect.apply(readU86, callResult5, []);
  return callResult7;
}

/** Флаг «клип не вращается». */
function isDoNotRotateClip({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const getRendering2 = localValue01["getRendering"];
  const callResult3 = Reflect.apply(getRendering2, localValue01, []);
  const capturedValue04 = capturedValue0;
  const DO_NOT_ROTATE_THE_CLIP5 = capturedValue04["DO_NOT_ROTATE_THE_CLIP"];
  const result6 = callResult3 === DO_NOT_ROTATE_THE_CLIP5;
  return result6;
}

/** Снаряд отскакивает. */
function isBouncing({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const Bouncing6 = LogicProjectileData5["Bouncing"];
  const callResult7 = Reflect.apply(add3, instance2, [Bouncing6]);
  const readU88 = callResult7["readU8"];
  const callResult9 = Reflect.apply(readU88, callResult7, []);
  const result10 = !callResult9;
  const result11 = !result10;
  return result11;
}

/** Снаряд летит непрямо (по пути/по площади). */
function isIndirect() {
  let v2;
  const localValue21 = v2;
  const getChargeType2 = localValue21["getChargeType"];
  const callResult3 = Reflect.apply(getChargeType2, localValue21, []);
  const localValue24 = v2;
  const getProjectile5 = localValue24["getProjectile"];
  const callResult6 = Reflect.apply(getProjectile5, localValue24, []);
  const projectile7 = projectile;
  const instance8 = projectile7["instance"];
  const isNull9 = instance8["isNull"];
  const callResult10 = Reflect.apply(isNull9, instance8, []);
  const result11 = !callResult10;
  if (!result11) {
  const chargeType12 = chargeType;
  const result13 = chargeType12 == 20;
  if (result13) {
  if (!result13) {
  return result13;
  } else {
  const localValue214 = v2;
  const getProjectile15 = localValue214["getProjectile"];
  const callResult16 = Reflect.apply(getProjectile15, localValue214, []);
  const instance17 = callResult16["instance"];
  const isNull18 = instance17["isNull"];
  const callResult19 = Reflect.apply(isNull18, instance17, []);
  return callResult19;
  } else {
  const chargeType20 = chargeType;
  const result21 = chargeType20 == 26;
  if (result21) {
  if (!result21) {
  return result21;
  } else {
  const localValue222 = v2;
  const getProjectile23 = localValue222["getProjectile"];
  const callResult24 = Reflect.apply(getProjectile23, localValue222, []);
  const instance25 = callResult24["instance"];
  const isNull26 = instance25["isNull"];
  const callResult27 = Reflect.apply(isNull26, instance25, []);
  return callResult27;
  } else {
  const chargeType28 = chargeType;
  const result29 = chargeType28 == 12;
  if (result29) {
  if (!result29) {
  return result29;
  } else {
  const localValue230 = v2;
  const getProjectile31 = localValue230["getProjectile"];
  const callResult32 = Reflect.apply(getProjectile31, localValue230, []);
  const instance33 = callResult32["instance"];
  const isNull34 = instance33["isNull"];
  const callResult35 = Reflect.apply(isNull34, instance33, []);
  return callResult35;
  } else {
  const chargeType36 = chargeType;
  const result37 = chargeType36 == 10;
  if (result37) {
  if (!result37) {
  return result37;
  } else {
  const localValue238 = v2;
  const getProjectile39 = localValue238["getProjectile"];
  const callResult40 = Reflect.apply(getProjectile39, localValue238, []);
  const instance41 = callResult40["instance"];
  const isNull42 = instance41["isNull"];
  const callResult43 = Reflect.apply(isNull42, instance41, []);
  return callResult43;
  } else {
  const chargeType44 = chargeType;
  const result45 = chargeType44 == 18;
  if (!result45) {
  return result45;
  } else {
  const localValue246 = v2;
  const getProjectile47 = localValue246["getProjectile"];
  const callResult48 = Reflect.apply(getProjectile47, localValue246, []);
  const instance49 = callResult48["instance"];
  const isNull50 = instance49["isNull"];
  const callResult51 = Reflect.apply(isNull50, instance49, []);
  return callResult51;
  } else {
  const projectile52 = projectile;
  const getIndirect53 = projectile52["getIndirect"];
  const callResult54 = Reflect.apply(getIndirect53, projectile52, []);
  if (!callResult54) {
  const chargeType55 = chargeType;
  const result56 = chargeType55 == 20;
  if (result56) {
  if (!result56) {
  return result56;
  } else {
  const localValue257 = v2;
  const getProjectile58 = localValue257["getProjectile"];
  const callResult59 = Reflect.apply(getProjectile58, localValue257, []);
  const instance60 = callResult59["instance"];
  const isNull61 = instance60["isNull"];
  const callResult62 = Reflect.apply(isNull61, instance60, []);
  return callResult62;
  } else {
  const chargeType63 = chargeType;
  const result64 = chargeType63 == 26;
  if (result64) {
  if (!result64) {
  return result64;
  } else {
  const localValue265 = v2;
  const getProjectile66 = localValue265["getProjectile"];
  const callResult67 = Reflect.apply(getProjectile66, localValue265, []);
  const instance68 = callResult67["instance"];
  const isNull69 = instance68["isNull"];
  const callResult70 = Reflect.apply(isNull69, instance68, []);
  return callResult70;
  } else {
  const chargeType71 = chargeType;
  const result72 = chargeType71 == 12;
  if (result72) {
  if (!result72) {
  return result72;
  } else {
  const localValue273 = v2;
  const getProjectile74 = localValue273["getProjectile"];
  const callResult75 = Reflect.apply(getProjectile74, localValue273, []);
  const instance76 = callResult75["instance"];
  const isNull77 = instance76["isNull"];
  const callResult78 = Reflect.apply(isNull77, instance76, []);
  return callResult78;
  } else {
  const chargeType79 = chargeType;
  const result80 = chargeType79 == 10;
  if (result80) {
  if (!result80) {
  return result80;
  } else {
  const localValue281 = v2;
  const getProjectile82 = localValue281["getProjectile"];
  const callResult83 = Reflect.apply(getProjectile82, localValue281, []);
  const instance84 = callResult83["instance"];
  const isNull85 = instance84["isNull"];
  const callResult86 = Reflect.apply(isNull85, instance84, []);
  return callResult86;
  } else {
  const chargeType87 = chargeType;
  const result88 = chargeType87 == 18;
  if (!result88) {
  return result88;
  } else {
  const localValue289 = v2;
  const getProjectile90 = localValue289["getProjectile"];
  const callResult91 = Reflect.apply(getProjectile90, localValue289, []);
  const instance92 = callResult91["instance"];
  const isNull93 = instance92["isNull"];
  const callResult94 = Reflect.apply(isNull93, instance92, []);
  return callResult94;
  } else {
  const arrayValue95 = [];
  const result96 = !arrayValue95;
  const result97 = !result96;
  return result97;
}

/** Данные непрямого снаряда. */
function getIndirect({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const Indirect6 = LogicProjectileData5["Indirect"];
  const callResult7 = Reflect.apply(add3, instance2, [Indirect6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  const result10 = !callResult9;
  const result11 = !result10;
  return result11;
}

/** Снаряд проходит сквозь окружение. */
function getPassesEnvironment({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const PiercesEnvironment6 = LogicProjectileData5["PiercesEnvironment"];
  const callResult7 = Reflect.apply(add3, instance2, [PiercesEnvironment6]);
  const readU88 = callResult7["readU8"];
  const callResult9 = Reflect.apply(readU88, callResult7, []);
  const result10 = !callResult9;
  const result11 = !result10;
  if (result11) {
  return result11;
  } else {
  const localValue012 = v0;
  const instance13 = localValue012["instance"];
  const add14 = instance13["add"];
  const capturedValue015 = capturedValue0;
  const LogicProjectileData16 = capturedValue015["LogicProjectileData"];
  const PiercesEnvironmentLikeButter17 = LogicProjectileData16["PiercesEnvironmentLikeButter"];
  const callResult18 = Reflect.apply(add14, instance13, [PiercesEnvironmentLikeButter17]);
  const readU819 = callResult18["readU8"];
  const callResult20 = Reflect.apply(readU819, callResult18, []);
  const result21 = !callResult20;
  const result22 = !result21;
  if (result22) {
  return result22;
  } else {
  const localValue023 = v0;
  const instance24 = localValue023["instance"];
  const add25 = instance24["add"];
  const capturedValue026 = capturedValue0;
  const LogicProjectileData27 = capturedValue026["LogicProjectileData"];
  const PassesEnvironment28 = LogicProjectileData27["PassesEnvironment"];
  const callResult29 = Reflect.apply(add25, instance24, [PassesEnvironment28]);
  const readU830 = callResult29["readU8"];
  const callResult31 = Reflect.apply(readU830, callResult29, []);
  const result32 = !callResult31;
  const result33 = !result32;
  return result33;
}

/** Объект area-effect при спавне. */
function getSpawnAreaEffectObject({capturedValue0, capturedValue1}) {
  let v0;
  const capturedValue01 = capturedValue0;
  const localValue02 = v0;
  const instance3 = localValue02["instance"];
  const add4 = instance3["add"];
  const capturedValue15 = capturedValue1;
  const LogicProjectileData6 = capturedValue15["LogicProjectileData"];
  const SpawnAreaEffectObject7 = LogicProjectileData6["SpawnAreaEffectObject"];
  const callResult8 = Reflect.apply(add4, instance3, [SpawnAreaEffectObject7]);
  const readPointer9 = callResult8["readPointer"];
  const callResult10 = Reflect.apply(readPointer9, callResult8, []);
  const callResult11 = Reflect.construct(capturedValue01, [callResult10], capturedValue01);
  return callResult11;
}

/** Время жизни «руления» снаряда. */
function getSteeringLifeTime({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const SteeringLifeTime6 = LogicProjectileData5["SteeringLifeTime"];
  const callResult7 = Reflect.apply(add3, instance2, [SteeringLifeTime6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Суммарная дельта угла. */
function getTotalDelta({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectile5 = capturedValue04["LogicProjectile"];
  const TotalDelta6 = LogicProjectile5["TotalDelta"];
  const callResult7 = Reflect.apply(add3, instance2, [TotalDelta6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Разница углов по кратчайшей дуге. */
function getAngleDifference(arg0, arg1) {
  let v0;
  const Math1 = Math;
  const abs2 = Math1["abs"];
  const argument03 = arg0;
  const argument14 = arg1;
  const result5 = argument03 - argument14;
  const result6 = result5 % 360;
  const callResult7 = Reflect.apply(abs2, Math1, [result6]);
  const localValue08 = v0;
  const result9 = localValue08 > 180;
  if (!result9) {
  const localValue010 = v0;
  return localValue010;
  } else {
  const localValue011 = v0;
  const result12 = 360 - localValue011;
  return result12;
}

/** Градусы → радианы. */
function getRadians(arg0) {
  const argument01 = arg0;
  const Math2 = Math;
  const PI3 = Math2["PI"];
  const result4 = PI3 / 180;
  const result5 = argument01 * result4;
  return result5;
}

/** Угол в градусах от точки к точке. */
function getDegreesToPoint(arg0, arg1, arg2, arg3) {
  let v0;
  const localValue01 = v0;
  const getDegrees2 = localValue01["getDegrees"];
  const localValue03 = v0;
  const getRadiansBetweenPoints4 = localValue03["getRadiansBetweenPoints"];
  const argument05 = arg0;
  const argument16 = arg1;
  const argument27 = arg2;
  const argument38 = arg3;
  const callResult9 = Reflect.apply(getRadiansBetweenPoints4, localValue03, [argument05, argument16, argument27, argument38]);
  const callResult10 = Reflect.apply(getDegrees2, localValue01, [callResult9]);
  return callResult10;
}

/** Евклидово расстояние. */
function getDistance(originX, originY, targetX, targetY) {
  let v0, v1;
  const targetX1 = targetX;
  const originX2 = originX;
  const result3 = targetX1 - originX2;
  const targetY4 = targetY;
  const originY5 = originY;
  const result6 = targetY4 - originY5;
  const Math7 = Math;
  const sqrt8 = Math7["sqrt"];
  const localValue09 = v0;
  const localValue010 = v0;
  const result11 = localValue09 * localValue010;
  const localValue112 = v1;
  const localValue113 = v1;
  const result14 = localValue112 * localValue113;
  const result15 = result11 + result14;
  const callResult16 = Reflect.apply(sqrt8, Math7, [result15]);
  return callResult16;
}

/** Swept-circle: пересечение движущихся окружностей на отрезке [0,1]. */
function checkSweptCollision(startPosition, endPosition, projectileStart, projectileEnd, combinedRadius) {
  const startPosition1 = startPosition;
  const propertyValue2 = startPosition1[0];
  const projectileStart3 = projectileStart;
  const propertyValue4 = projectileStart3[0];
  const result5 = propertyValue2 - propertyValue4;
  const startPosition6 = startPosition;
  const propertyValue7 = startPosition6[1];
  const projectileStart8 = projectileStart;
  const propertyValue9 = projectileStart8[1];
  const result10 = propertyValue7 - propertyValue9;
  const endPosition11 = endPosition;
  const propertyValue12 = endPosition11[0];
  const startPosition13 = startPosition;
  const propertyValue14 = startPosition13[0];
  const result15 = propertyValue12 - propertyValue14;
  const projectileEnd16 = projectileEnd;
  const propertyValue17 = projectileEnd16[0];
  const projectileStart18 = projectileStart;
  const propertyValue19 = projectileStart18[0];
  const result20 = propertyValue17 - propertyValue19;
  const result21 = result15 - result20;
  const endPosition22 = endPosition;
  const propertyValue23 = endPosition22[1];
  const startPosition24 = startPosition;
  const propertyValue25 = startPosition24[1];
  const result26 = propertyValue23 - propertyValue25;
  const projectileEnd27 = projectileEnd;
  const propertyValue28 = projectileEnd27[1];
  const projectileStart29 = projectileStart;
  const propertyValue30 = projectileStart29[1];
  const result31 = propertyValue28 - propertyValue30;
  const result32 = result26 - result31;
  const relativeX33 = relativeX;
  const relativeX34 = relativeX;
  const result35 = relativeX33 * relativeX34;
  const relativeY36 = relativeY;
  const relativeY37 = relativeY;
  const result38 = relativeY36 * relativeY37;
  const result39 = result35 + result38;
  const combinedRadius40 = combinedRadius;
  const combinedRadius41 = combinedRadius;
  const result42 = combinedRadius40 * combinedRadius41;
  const result43 = result39 - result42;
  const initialSeparation44 = initialSeparation;
  const result45 = initialSeparation44 <= 0;
  if (!result45) {
  const relativeVelocityX46 = relativeVelocityX;
  const relativeVelocityX47 = relativeVelocityX;
  const result48 = relativeVelocityX46 * relativeVelocityX47;
  const relativeVelocityY49 = relativeVelocityY;
  const relativeVelocityY50 = relativeVelocityY;
  const result51 = relativeVelocityY49 * relativeVelocityY50;
  const result52 = result48 + result51;
  const relativeSpeedSquared53 = relativeSpeedSquared;
  const result54 = relativeSpeedSquared53 == 0;
  if (!result54) {
  const relativeX55 = relativeX;
  const relativeVelocityX56 = relativeVelocityX;
  const result57 = relativeX55 * relativeVelocityX56;
  const relativeY58 = relativeY;
  const relativeVelocityY59 = relativeVelocityY;
  const result60 = relativeY58 * relativeVelocityY59;
  const result61 = result57 + result60;
  const result62 = 2 * result61;
  const quadraticLinear63 = quadraticLinear;
  const quadraticLinear64 = quadraticLinear;
  const result65 = quadraticLinear63 * quadraticLinear64;
  const relativeSpeedSquared66 = relativeSpeedSquared;
  const result67 = 4 * relativeSpeedSquared66;
  const initialSeparation68 = initialSeparation;
  const result69 = result67 * initialSeparation68;
  const result70 = result65 - result69;
  const discriminant71 = discriminant;
  const result72 = discriminant71 < 0;
  if (!result72) {
  const Math73 = Math;
  const sqrt74 = Math73["sqrt"];
  const discriminant75 = discriminant;
  const callResult76 = Reflect.apply(sqrt74, Math73, [discriminant75]);
  const quadraticLinear77 = quadraticLinear;
  const result78 = -quadraticLinear77;
  const sqrtDiscriminant79 = sqrtDiscriminant;
  const result80 = result78 - sqrtDiscriminant79;
  const relativeSpeedSquared81 = relativeSpeedSquared;
  const result82 = 2 * relativeSpeedSquared81;
  const result83 = result80 / result82;
  const quadraticLinear84 = quadraticLinear;
  const result85 = -quadraticLinear84;
  const sqrtDiscriminant86 = sqrtDiscriminant;
  const result87 = result85 + sqrtDiscriminant86;
  const relativeSpeedSquared88 = relativeSpeedSquared;
  const result89 = 2 * relativeSpeedSquared88;
  const result90 = result87 / result89;
  const exitTime91 = exitTime;
  const result92 = exitTime91 < 0;
  if (result92) {
  if (!result92) {
  const arrayValue93 = [];
  const result94 = !arrayValue93;
  const result95 = !result94;
  return result95;
  } else {
  const arrayValue96 = [];
  const result97 = !arrayValue96;
  return result97;
  } else {
  const enterTime98 = enterTime;
  const result99 = enterTime98 > 1;
  if (!result99) {
  const arrayValue100 = [];
  const result101 = !arrayValue100;
  const result102 = !result101;
  return result102;
  } else {
  const arrayValue103 = [];
  const result104 = !arrayValue103;
  return result104;
  } else {
  const arrayValue105 = [];
  const result106 = !arrayValue105;
  return result106;
  } else {
  const arrayValue107 = [];
  const result108 = !arrayValue107;
  return result108;
  } else {
  const arrayValue109 = [];
  const result110 = !arrayValue109;
  const result111 = !result110;
  return result111;
}

/** Урон с учётом динамических модификаторов. */
function getDynamicDamage(arg0, arg1) {
  let v0, v1, v2, v4;
  const BattleCache1 = BattleCache;
  const TickPosition2 = BattleCache1["TickPosition"];
  if (TickPosition2) {
  const BattleCache3 = BattleCache;
  const CurrentSpeed4 = BattleCache3["CurrentSpeed"];
  const localValue15 = v1;
  const result6 = localValue15 / 20;
  const localValue47 = v4;
  const getPathByAngle8 = localValue47["getPathByAngle"];
  const localValue09 = v0;
  const argument010 = arg0;
  const localValue211 = v2;
  const callResult12 = Reflect.apply(getPathByAngle8, localValue47, [localValue09, argument010, localValue211]);
  const localValue413 = v4;
  const getPathDamage14 = localValue413["getPathDamage"];
  const pathByAngle15 = pathByAngle;
  const argument116 = arg1;
  const callResult17 = Reflect.apply(getPathDamage14, localValue413, [pathByAngle15, argument116]);
  return callResult17;
  } else {
  const BattleCache18 = BattleCache;
  const ServerPosition19 = BattleCache18["ServerPosition"];
  const BattleCache20 = BattleCache;
  const CurrentSpeed21 = BattleCache20["CurrentSpeed"];
  const localValue122 = v1;
  const result23 = localValue122 / 20;
  const localValue424 = v4;
  const getPathByAngle25 = localValue424["getPathByAngle"];
  const localValue026 = v0;
  const argument027 = arg0;
  const localValue228 = v2;
  const callResult29 = Reflect.apply(getPathByAngle25, localValue424, [localValue026, argument027, localValue228]);
  const localValue430 = v4;
  const getPathDamage31 = localValue430["getPathDamage"];
  const pathByAngle32 = pathByAngle;
  const argument133 = arg1;
  const callResult34 = Reflect.apply(getPathDamage31, localValue430, [pathByAngle32, argument133]);
  return callResult34;
}

/** Есть ли результат у записи урона. */
function hasResult() {
  let v0;
  const localValue01 = v0;
  const completed2 = localValue01["completed"];
  const result3 = completed2 !== null;
  return result3;
}

/** Снаряд привязан к позиции персонажа. */
function isCharacterPositionBased() {
  let v1;
  const localValue11 = v1;
  const getBaseName2 = localValue11["getBaseName"];
  const callResult3 = Reflect.apply(getBaseName2, localValue11, []);
  const baseName4 = baseName;
  const includes5 = baseName4["includes"];
  const callResult6 = Reflect.apply(includes5, baseName4, ["Circling"]);
  if (callResult6) {
  return callResult6;
  } else {
  const baseName7 = baseName;
  const result8 = baseName7 == "AxeJugglerProjectile2";
  if (result8) {
  return result8;
  } else {
  const baseName9 = baseName;
  const result10 = baseName9 == "AxeJugglerOverchargedProjectile2";
  return result10;
}

/** Снаряд мой. */
function isOwn(arg0, {captured_getInstance}) {
  let v0, v4;
  const argument01 = arg0;
  const result2 = argument01 < 0;
  if (!result2) {
  const captured_getInstance3 = captured_getInstance;
  const callResult4 = Reflect.construct(captured_getInstance3, [], captured_getInstance3);
  const localValue05 = v0;
  const getLogicPlayer6 = localValue05["getLogicPlayer"];
  const argument07 = arg0;
  const callResult8 = Reflect.apply(getLogicPlayer6, localValue05, [argument07]);
  const localValue09 = v0;
  const getOwnPlayerIndex10 = localValue09["getOwnPlayerIndex"];
  const callResult11 = Reflect.apply(getOwnPlayerIndex10, localValue09, []);
  const logicPlayer12 = logicPlayer;
  const getPlayerIndex13 = logicPlayer12["getPlayerIndex"];
  const callResult14 = Reflect.apply(getPlayerIndex13, logicPlayer12, []);
  const ownPlayerIndex15 = ownPlayerIndex;
  const playerIndex16 = playerIndex;
  const result17 = ownPlayerIndex15 == playerIndex16;
  const localValue418 = v4;
  const result19 = !localValue418;
  if (!result19) {
  const arrayValue20 = [];
  const result21 = !arrayValue20;
  const result22 = !result21;
  return result22;
  } else {
  const arrayValue23 = [];
  const result24 = !arrayValue23;
  return result24;
  } else {
  const arrayValue25 = [];
  const result26 = !arrayValue25;
  return result26;
}

/** Снаряд своей команды. */
function isOwnTeam(arg0, {captured_getInstance, captured_45_FRIENDLY_FIRE}) {
  let v0, v4;
  const argument01 = arg0;
  const result2 = argument01 < 0;
  if (!result2) {
  const captured_getInstance3 = captured_getInstance;
  const callResult4 = Reflect.construct(captured_getInstance3, [], captured_getInstance3);
  const localValue05 = v0;
  const getEventModifier6 = localValue05["getEventModifier"];
  const captured_45_FRIENDLY_FIRE7 = captured_45_FRIENDLY_FIRE;
  const FRIENDLY_FIRE8 = captured_45_FRIENDLY_FIRE7["FRIENDLY_FIRE"];
  const callResult9 = Reflect.apply(getEventModifier6, localValue05, [FRIENDLY_FIRE8]);
  if (!callResult9) {
  const localValue010 = v0;
  const getLogicPlayer11 = localValue010["getLogicPlayer"];
  const argument012 = arg0;
  const callResult13 = Reflect.apply(getLogicPlayer11, localValue010, [argument012]);
  const localValue014 = v0;
  const getOwnPlayerTeam15 = localValue014["getOwnPlayerTeam"];
  const callResult16 = Reflect.apply(getOwnPlayerTeam15, localValue014, []);
  const logicPlayer17 = logicPlayer;
  const getPlayerTeam18 = logicPlayer17["getPlayerTeam"];
  const callResult19 = Reflect.apply(getPlayerTeam18, logicPlayer17, []);
  const ownPlayerTeam20 = ownPlayerTeam;
  const playerTeam21 = playerTeam;
  const result22 = ownPlayerTeam20 == playerTeam21;
  const localValue423 = v4;
  const result24 = !localValue423;
  if (!result24) {
  const arrayValue25 = [];
  const result26 = !arrayValue25;
  const result27 = !result26;
  return result27;
  } else {
  const arrayValue28 = [];
  const result29 = !arrayValue28;
  return result29;
  } else {
  const arrayValue30 = [];
  const result31 = !arrayValue30;
  return result31;
  } else {
  const arrayValue32 = [];
  const result33 = !arrayValue32;
  return result33;
}

/** Текущий персонаж из BattleCache. */
function getCurrentCharacter(arg0, {capturedValue0}) {
  let v1, v3, v4;
  const argument01 = arg0;
  const condition2 = argument01 === undefined;
  if (!condition2) {
  const argument03 = arg0;
  if (!argument03) {
  const localValue44 = v4;
  const getLogicGameObjectManager5 = localValue44["getLogicGameObjectManager"];
  const callResult6 = Reflect.apply(getLogicGameObjectManager5, localValue44, []);
  const localValue47 = v4;
  const getControlledCharacterGlobalID8 = localValue47["getControlledCharacterGlobalID"];
  const callResult9 = Reflect.apply(getControlledCharacterGlobalID8, localValue47, []);
  if (callResult9) {
  const logicGameObjectManager10 = logicGameObjectManager;
  const findGameObject11 = logicGameObjectManager10["findGameObject"];
  const localValue312 = v3;
  const callResult13 = Reflect.apply(findGameObject11, logicGameObjectManager10, [localValue312]);
  const capturedValue014 = capturedValue0;
  const localValue115 = v1;
  const callResult16 = Reflect.construct(capturedValue014, [localValue115], capturedValue014);
  return callResult16;
  } else {
  const localValue417 = v4;
  const getOwnCharacterGlobalID18 = localValue417["getOwnCharacterGlobalID"];
  const callResult19 = Reflect.apply(getOwnCharacterGlobalID18, localValue417, []);
  const logicGameObjectManager20 = logicGameObjectManager;
  const findGameObject21 = logicGameObjectManager20["findGameObject"];
  const localValue322 = v3;
  const callResult23 = Reflect.apply(findGameObject21, logicGameObjectManager20, [localValue322]);
  const capturedValue024 = capturedValue0;
  const localValue125 = v1;
  const callResult26 = Reflect.construct(capturedValue024, [localValue125], capturedValue024);
  return callResult26;
  } else {
  const BattleCache27 = BattleCache;
  const CurrentCharacter28 = BattleCache27["CurrentCharacter"];
  if (CurrentCharacter28) {
  const capturedValue029 = capturedValue0;
  const localValue130 = v1;
  const callResult31 = Reflect.construct(capturedValue029, [localValue130], capturedValue029);
  return callResult31;
  } else {
  const ptr32 = ptr;
  const callResult33 = Reflect.apply(ptr32, undefined, [0]);
  const capturedValue034 = capturedValue0;
  const localValue135 = v1;
  const callResult36 = Reflect.construct(capturedValue034, [localValue135], capturedValue034);
  return callResult36;
  } else {
  const arrayValue37 = [];
  const result38 = !arrayValue37;
  const result39 = !result38;
  const argument040 = arg0;
  if (!argument040) {
  const localValue441 = v4;
  const getLogicGameObjectManager42 = localValue441["getLogicGameObjectManager"];
  const callResult43 = Reflect.apply(getLogicGameObjectManager42, localValue441, []);
  const localValue444 = v4;
  const getControlledCharacterGlobalID45 = localValue444["getControlledCharacterGlobalID"];
  const callResult46 = Reflect.apply(getControlledCharacterGlobalID45, localValue444, []);
  if (callResult46) {
  const logicGameObjectManager47 = logicGameObjectManager;
  const findGameObject48 = logicGameObjectManager47["findGameObject"];
  const localValue349 = v3;
  const callResult50 = Reflect.apply(findGameObject48, logicGameObjectManager47, [localValue349]);
  const capturedValue051 = capturedValue0;
  const localValue152 = v1;
  const callResult53 = Reflect.construct(capturedValue051, [localValue152], capturedValue051);
  return callResult53;
  } else {
  const localValue454 = v4;
  const getOwnCharacterGlobalID55 = localValue454["getOwnCharacterGlobalID"];
  const callResult56 = Reflect.apply(getOwnCharacterGlobalID55, localValue454, []);
  const logicGameObjectManager57 = logicGameObjectManager;
  const findGameObject58 = logicGameObjectManager57["findGameObject"];
  const localValue359 = v3;
  const callResult60 = Reflect.apply(findGameObject58, logicGameObjectManager57, [localValue359]);
  const capturedValue061 = capturedValue0;
  const localValue162 = v1;
  const callResult63 = Reflect.construct(capturedValue061, [localValue162], capturedValue061);
  return callResult63;
  } else {
  const BattleCache64 = BattleCache;
  const CurrentCharacter65 = BattleCache64["CurrentCharacter"];
  if (CurrentCharacter65) {
  const capturedValue066 = capturedValue0;
  const localValue167 = v1;
  const callResult68 = Reflect.construct(capturedValue066, [localValue167], capturedValue066);
  return callResult68;
  } else {
  const ptr69 = ptr;
  const callResult70 = Reflect.apply(ptr69, undefined, [0]);
  const capturedValue071 = capturedValue0;
  const localValue172 = v1;
  const callResult73 = Reflect.construct(capturedValue071, [localValue172], capturedValue071);
  return callResult73;
}

/** ID объекта. */
function getGlobalID({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicGameObject5 = capturedValue04["LogicGameObject"];
  const GlobalID6 = LogicGameObject5["GlobalID"];
  const callResult7 = Reflect.apply(add3, instance2, [GlobalID6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Индекс в списке. */
function getIndex({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicGameObject5 = capturedValue04["LogicGameObject"];
  const Index6 = LogicGameObject5["Index"];
  const callResult7 = Reflect.apply(add3, instance2, [Index6]);
  const readInt8 = callResult7["readInt"];
  const callResult9 = Reflect.apply(readInt8, callResult7, []);
  return callResult9;
}

/** Включён ли мод/функция. */
function isModEnabled(arg0, {capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const isModAllowed2 = localValue01["isModAllowed"];
  const argument03 = arg0;
  const callResult4 = Reflect.apply(isModAllowed2, localValue01, [argument03]);
  const result5 = !callResult4;
  if (!result5) {
  const localValue06 = v0;
  const getModFlags7 = localValue06["getModFlags"];
  const argument08 = arg0;
  const callResult9 = Reflect.apply(getModFlags7, localValue06, [argument08]);
  const Enabled10 = callResult9["Enabled"];
  const result11 = !Enabled10;
  if (!result11) {
  const capturedValue012 = capturedValue0;
  const Settings13 = capturedValue012["Settings"];
  const AdvancedConfigs14 = Settings13["AdvancedConfigs"];
  if (!AdvancedConfigs14) {
  const arrayValue15 = [];
  const result16 = !arrayValue15;
  const result17 = !result16;
  return result17;
  } else {
  const localValue018 = v0;
  const hasConfigs19 = localValue018["hasConfigs"];
  const argument020 = arg0;
  const callResult21 = Reflect.apply(hasConfigs19, localValue018, [argument020]);
  if (!callResult21) {
  const arrayValue22 = [];
  const result23 = !arrayValue22;
  const result24 = !result23;
  return result24;
  } else {
  const BattleCache25 = BattleCache;
  const CharacterName26 = BattleCache25["CharacterName"];
  const result27 = !CharacterName26;
  if (!result27) {
  const localValue028 = v0;
  const getConfigByBrawler29 = localValue028["getConfigByBrawler"];
  const argument030 = arg0;
  const BattleCache31 = BattleCache;
  const CharacterName32 = BattleCache31["CharacterName"];
  const callResult33 = Reflect.apply(getConfigByBrawler29, localValue028, [argument030, CharacterName32]);
  const result34 = callResult33 !== undefined;
  return result34;
  } else {
  const arrayValue35 = [];
  const result36 = !arrayValue35;
  return result36;
  } else {
  const arrayValue37 = [];
  const result38 = !arrayValue37;
  return result38;
  } else {
  const arrayValue39 = [];
  const result40 = !arrayValue39;
  return result40;
}

/** Чтение конфига модуля. */
function getConfig(arg0, {capturedValue0}) {
  let v0, v2;
  const localValue21 = v2;
  const hasConfigs2 = localValue21["hasConfigs"];
  const argument03 = arg0;
  const callResult4 = Reflect.apply(hasConfigs2, localValue21, [argument03]);
  if (!callResult4) {
  const localValue25 = v2;
  const getCurrentProfile6 = localValue25["getCurrentProfile"];
  const callResult7 = Reflect.apply(getCurrentProfile6, localValue25, []);
  const currentProfile8 = currentProfile;
  const condition9 = currentProfile8 === undefined || currentProfile8 === null;
  if (!condition9) {
  const mod_settings10 = currentProfile8["mod_settings"];
  const condition11 = mod_settings10 === undefined || mod_settings10 === null;
  if (!condition11) {
  const argument012 = arg0;
  const propertyValue13 = mod_settings10[argument012];
  const condition14 = propertyValue13 === undefined || propertyValue13 === null;
  if (!condition14) {
  return propertyValue13;
  } else {
  const capturedValue015 = capturedValue0;
  const argument016 = arg0;
  const propertyValue17 = capturedValue015[argument016];
  const Settings18 = propertyValue17["Settings"];
  return Settings18;
  } else {
  const condition19 = undefined === undefined || undefined === null;
  if (!condition19) {
  return undefined;
  } else {
  const capturedValue020 = capturedValue0;
  const argument021 = arg0;
  const propertyValue22 = capturedValue020[argument021];
  const Settings23 = propertyValue22["Settings"];
  return Settings23;
  } else {
  const condition24 = undefined === undefined || undefined === null;
  if (!condition24) {
  return undefined;
  } else {
  const capturedValue025 = capturedValue0;
  const argument026 = arg0;
  const propertyValue27 = capturedValue025[argument026];
  const Settings28 = propertyValue27["Settings"];
  return Settings28;
  } else {
  const BattleCache29 = BattleCache;
  const CharacterName30 = BattleCache29["CharacterName"];
  if (!CharacterName30) {
  const capturedValue031 = capturedValue0;
  const argument032 = arg0;
  const propertyValue33 = capturedValue031[argument032];
  const Settings34 = propertyValue33["Settings"];
  return Settings34;
  } else {
  const localValue235 = v2;
  const getConfigByBrawler36 = localValue235["getConfigByBrawler"];
  const argument037 = arg0;
  const BattleCache38 = BattleCache;
  const CharacterName39 = BattleCache38["CharacterName"];
  const callResult40 = Reflect.apply(getConfigByBrawler36, localValue235, [argument037, CharacterName39]);
  const localValue041 = v0;
  if (!localValue041) {
  const capturedValue042 = capturedValue0;
  const argument043 = arg0;
  const propertyValue44 = capturedValue042[argument043];
  const Settings45 = propertyValue44["Settings"];
  return Settings45;
  } else {
  const localValue046 = v0;
  return localValue046;
}

/** Тач по стороне экрана. */
function getTouchIdBySide(arg0, arg1) {
  let v0, v1, v2;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v2 = this;
    if (!arg1) { pc = 17; continue; }
    pc = 8; continue;
  }
    case 8: {
    pc = 24; continue;
  }
    case 17: {
    pc = 24; continue;
  }
    case 24: {
    v0 = v2["sideTouchIds"];
    pc = 30; continue;
  }
    case 30: {
    pc = 61; continue;
  }
    case 36: {
    v1 = inIterator(v0);
    pc = 39; continue;
  }
    case 39: {
    if (!v0[v1] == arg0) { pc = 61; continue; }
    pc = 50; continue;
  }
    case 50: {
    /* return Number.call(undefined, v1) */
  }
    case 61: {
    const [t1, t2] = inStep(v2["originalSideTouchIds"]);
    if (!t2) { pc = 36; continue; }
    pc = 64; continue;
  }
    case 64: {
    return;
  }
    }
  }
}

/** Найти сохранённый тач. */
function findStoredTouch(arg0) {
  let v0;
  const localValue01 = v0;
  const touches2 = localValue01["touches"];
  const find3 = touches2["find"];
  const callback4 = frame.makeClosure(0, {args, locals, refs, frame});
  const callResult5 = Reflect.apply(find3, touches2, [callback4]);
  return callResult5;
}

/** X фона стика движения. */
function getMoveStickBgX({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const BattleScreen5 = capturedValue04["BattleScreen"];
  const MoveStickBgX6 = BattleScreen5["MoveStickBgX"];
  const callResult7 = Reflect.apply(add3, instance2, [MoveStickBgX6]);
  const readFloat8 = callResult7["readFloat"];
  const callResult9 = Reflect.apply(readFloat8, callResult7, []);
  return callResult9;
}

/** Y фона стика движения. */
function getMoveStickBgY({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const BattleScreen5 = capturedValue04["BattleScreen"];
  const MoveStickBgY6 = BattleScreen5["MoveStickBgY"];
  const callResult7 = Reflect.apply(add3, instance2, [MoveStickBgY6]);
  const readFloat8 = callResult7["readFloat"];
  const callResult9 = Reflect.apply(readFloat8, callResult7, []);
  return callResult9;
}

/** Обновление предсказания (predictionWorker). */
function updateDodgePrediction({configuration, predictionWorker}) {
  let v0;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v0 = this;
    /* // Preserve the receiver for a method call. */
    if (!!configuration.isModEnabled("AutoDodge")) { pc = 49; continue; }
    pc = 22; continue;
  }
    case 22: {
    if (!!BattleCache["AllowedLerpingDodge"]) { pc = 49; continue; }
    pc = 36; continue;
  }
    case 36: {
    /* // Preserve the receiver for a method call. */
    return;
  }
    case 49: {
    /* // Preserve the receiver for a method call. */
    if (![!v0.updateTouches()][0]) { pc = 124; continue; }
    pc = 63; continue;
  }
    case 63: {
    if (![BattleCache["AllowedLerpingDodge"]][0]) { pc = 106; continue; }
    pc = 78; continue;
  }
    case 78: {
    /* BattleCache[propertyKey("GoingBackToOriginalDodge")] = !![] */
    /* // Preserve the receiver for a method call. */
    pc = 106; continue;
  }
    case 106: {
    /* [BattleCache, propertyKey("AllowedLerpingDodge"), ![]][0][[BattleCache, propertyKey("AllowedLerpingDodge"), ![]][1]] = [BattleCache, propertyKey("AllowedLerpingDodge"), ![]][2] */
    pc = 124; continue;
  }
    case 124: {
    return;
  }
    }
  }
}
