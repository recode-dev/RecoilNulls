function ofIterator(obj) { return obj[Symbol.iterator](); }
function inIterator(obj) { return Object.keys(obj)[Symbol.iterator](); }
function ofStep(it) { const r = it.next(); return [r.value, r.done]; }
function inStep(it) { const r = it.next(); return [r.value, r.done]; }
function getBestPath(arg0, arg1, {mathBindings}) {
  let pathByAngle, v0, v1, v10, v2, v3, v4, v5, v6, v7, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v10 = this;
    if (!!Number.isFinite(arg0)) { pc = 46; continue; }
    pc = 33; continue;
  }
    case 33: {
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
  }
    case 149: {
    const [t1, t2] = inStep();
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
    v5 = [v5 + 360 / 16][0];
    pc = 207; continue;
  }
    case 251: {
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
    pathByAngle = v10.getPathByAngle(v2, v7, v3);
    if (!v6) { pc = 387; continue; }
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
    const [t3, t4] = ofStep();
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
function getPathDamage({capturedValue0, capturedValue1, capturedValue2, capturedValue3}) {
  let v0, v1, v2;
  function function_d63f5d05__d63f5d05827a34eb(input = {}) {
  const BattleCache1 = BattleCache;
  const CurrentPosition2 = BattleCache1["CurrentPosition"];
  if (CurrentPosition2) {
  v0 = CurrentPosition2;
  const localValue03 = v0;
  const result4 = !localValue03;
  if (!result4) {
  const capturedValue15 = capturedValue1;
  const MinDistance6 = capturedValue15["MinDistance"];
  const result7 = MinDistance6 * 300;
  v1 = result7;
  const localValue18 = v1;
  const result9 = localValue18 == 0;
  if (!result9) {
  const capturedValue210 = capturedValue2;
  const MathUtil11 = capturedValue210["MathUtil"];
  const getDistance12 = MathUtil11["getDistance"];
  const localValue013 = v0;
  const propertyValue14 = localValue013[0];
  const localValue015 = v0;
  const propertyValue16 = localValue015[1];
  const capturedValue317 = capturedValue3;
  const propertyValue18 = capturedValue317[0];
  const capturedValue319 = capturedValue3;
  const propertyValue20 = capturedValue319[1];
  const callResult21 = Reflect.apply(getDistance12, MathUtil11, [propertyValue14, propertyValue16, propertyValue18, propertyValue20]);
  v2 = callResult21;
  const localValue222 = v2;
  const localValue123 = v1;
  const result24 = localValue222 > localValue123;
  if (!result24) {
  const arrayValue25 = [];
  const result26 = !arrayValue25;
  write(refsSlots.capturedValue0, result26, "_check");
  return undefined;
  } else {
  const arrayValue27 = [];
  const result28 = !arrayValue27;
  const result29 = !result28;
  write(refsSlots.capturedValue0, result29, "_check");
  return undefined;
  }
  } else {
  const arrayValue30 = [];
  const result31 = !arrayValue30;
  const result32 = !result31;
  write(refsSlots.capturedValue0, result32, "_check");
  return undefined;
  }
  } else {
  const arrayValue33 = [];
  const result34 = !arrayValue33;
  write(refsSlots.capturedValue0, result34, "_check");
  return undefined;
  }
  } else {
  const BattleCache35 = BattleCache;
  const ServerPosition36 = BattleCache35["ServerPosition"];
  v0 = ServerPosition36;
  const localValue037 = v0;
  const result38 = !localValue037;
  if (!result38) {
  const capturedValue139 = capturedValue1;
  const MinDistance40 = capturedValue139["MinDistance"];
  const result41 = MinDistance40 * 300;
  v1 = result41;
  const localValue142 = v1;
  const result43 = localValue142 == 0;
  if (!result43) {
  const capturedValue244 = capturedValue2;
  const MathUtil45 = capturedValue244["MathUtil"];
  const getDistance46 = MathUtil45["getDistance"];
  const localValue047 = v0;
  const propertyValue48 = localValue047[0];
  const localValue049 = v0;
  const propertyValue50 = localValue049[1];
  const capturedValue351 = capturedValue3;
  const propertyValue52 = capturedValue351[0];
  const capturedValue353 = capturedValue3;
  const propertyValue54 = capturedValue353[1];
  const callResult55 = Reflect.apply(getDistance46, MathUtil45, [propertyValue48, propertyValue50, propertyValue52, propertyValue54]);
  v2 = callResult55;
  const localValue256 = v2;
  const localValue157 = v1;
  const result58 = localValue256 > localValue157;
  if (!result58) {
  const arrayValue59 = [];
  const result60 = !arrayValue59;
  write(refsSlots.capturedValue0, result60, "_check");
  return undefined;
  } else {
  const arrayValue61 = [];
  const result62 = !arrayValue61;
  const result63 = !result62;
  write(refsSlots.capturedValue0, result63, "_check");
  return undefined;
  }
  } else {
  const arrayValue64 = [];
  const result65 = !arrayValue64;
  const result66 = !result65;
  write(refsSlots.capturedValue0, result66, "_check");
  return undefined;
  }
  } else {
  const arrayValue67 = [];
  const result68 = !arrayValue67;
  write(refsSlots.capturedValue0, result68, "_check");
  return undefined;
  }
  }
  }
  const metadata57 = {"id":"f_d65a89009a961e44","bindings":["getPathDamage"],"argumentCount":2,"variableCount":33,"constants":[],"closures":[{"flags":9,"variableIndex":1320,"atom":0,"name":""},{"flags":13,"variableIndex":1182,"atom":0,"name":""},{"flags":1,"variableIndex":6,"atom":0,"name":""}]};
}
function getPathDamageBounded(startPosition, endPosition, projectileStart, projectileEnd, combinedRadius) {
  function checkSweptCollision__7b14b1912d56a4ae(input = {}) {
  const startPosition1 = startPosition;
  const propertyValue2 = startPosition1[0];
  const projectileStart3 = projectileStart;
  const propertyValue4 = projectileStart3[0];
  const result5 = propertyValue2 - propertyValue4;
  relativeX = result5;
  const startPosition6 = startPosition;
  const propertyValue7 = startPosition6[1];
  const projectileStart8 = projectileStart;
  const propertyValue9 = projectileStart8[1];
  const result10 = propertyValue7 - propertyValue9;
  relativeY = result10;
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
  relativeVelocityX = result21;
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
  relativeVelocityY = result32;
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
  initialSeparation = result43;
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
  relativeSpeedSquared = result52;
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
  quadraticLinear = result62;
  const quadraticLinear63 = quadraticLinear;
  const quadraticLinear64 = quadraticLinear;
  const result65 = quadraticLinear63 * quadraticLinear64;
  const relativeSpeedSquared66 = relativeSpeedSquared;
  const result67 = 4 * relativeSpeedSquared66;
  const initialSeparation68 = initialSeparation;
  const result69 = result67 * initialSeparation68;
  const result70 = result65 - result69;
  discriminant = result70;
  const discriminant71 = discriminant;
  const result72 = discriminant71 < 0;
  if (!result72) {
  const Math73 = Math;
  const sqrt74 = Math73["sqrt"];
  const discriminant75 = discriminant;
  const callResult76 = Reflect.apply(sqrt74, Math73, [discriminant75]);
  sqrtDiscriminant = callResult76;
  const quadraticLinear77 = quadraticLinear;
  const result78 = -quadraticLinear77;
  const sqrtDiscriminant79 = sqrtDiscriminant;
  const result80 = result78 - sqrtDiscriminant79;
  const relativeSpeedSquared81 = relativeSpeedSquared;
  const result82 = 2 * relativeSpeedSquared81;
  const result83 = result80 / result82;
  enterTime = result83;
  const quadraticLinear84 = quadraticLinear;
  const result85 = -quadraticLinear84;
  const sqrtDiscriminant86 = sqrtDiscriminant;
  const result87 = result85 + sqrtDiscriminant86;
  const relativeSpeedSquared88 = relativeSpeedSquared;
  const result89 = 2 * relativeSpeedSquared88;
  const result90 = result87 / result89;
  exitTime = result90;
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
  }
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
  }
  }
  } else {
  const arrayValue105 = [];
  const result106 = !arrayValue105;
  return result106;
  }
  } else {
  const arrayValue107 = [];
  const result108 = !arrayValue107;
  return result108;
  }
  } else {
  const arrayValue109 = [];
  const result110 = !arrayValue109;
  const result111 = !result110;
  return result111;
  }
  }
  const metadata37 = {"id":"f_7d06d20742aded15","bindings":["getPathDamageBounded"],"argumentCount":3,"variableCount":33,"constants":[],"closures":[{"flags":9,"variableIndex":1320,"atom":0,"name":""},{"flags":13,"variableIndex":1182,"atom":0,"name":""},{"flags":1,"variableIndex":6,"atom":0,"name":""}]};
}
function getStillDamage(arg0, arg1, arg2, {dodgePlanner, mathBindings}) {
  let v0, v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v3, v4, v5, v6, v7, v8, v9;
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
function getStillDamageOriginal(arg0, arg1) {
  let v0;
  function getAngleDifference__c9b274443706cbf2(input = {}) {
  const Math1 = Math;
  const abs2 = Math1["abs"];
  const argument03 = arg0;
  const argument14 = arg1;
  const result5 = argument03 - argument14;
  const result6 = result5 % 360;
  const callResult7 = Reflect.apply(abs2, Math1, [result6]);
  v0 = callResult7;
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
  }
  const metadata54 = {"id":"f_caf0bb77709b156f","bindings":["getStillDamageOriginal"],"argumentCount":3,"variableCount":20,"constants":[],"closures":[{"flags":9,"variableIndex":1320,"atom":0,"name":""},{"flags":13,"variableIndex":1182,"atom":0,"name":""},{"flags":1,"variableIndex":6,"atom":0,"name":""}]};
}
function getDamage(arg0, {configuration, dodgePlanner, mathBindings}) {
  let v0, v1, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v2, v20, v3, v4, v5, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v20 = this;
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
    v15 = mathBindings["MathUtil"].getDistance(v14[0], v14[1], v13[0], v13[1]);
    if (!v15 <= arg0 + v4["areaEffectRadius"] + v9) { pc = 839; continue; }
    pc = 516; continue;
  }
    case 516: {
    v2 = [v2 + v4["damage"]][0];
    pc = 846; continue;
  }
    case 537: {
    if (!v20.checkSweptCollision(v12, v11, v0, v0, arg0 + v4["radius"] + v9)) { pc = 674; continue; }
    pc = 577; continue;
  }
    case 577: {
    v2 = [v2 + v4["damage"]][0];
    if (!v4["damageOnlyWithOneProj"]) { pc = 846; continue; }
    pc = 609; continue;
  }
    case 609: {
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
    if (!v20.checkSweptCollision(v12, v11, v0, v0, arg0 + v4["areaEffectRadius"] + v9)) { pc = 839; continue; }
    pc = 746; continue;
  }
    case 746: {
    v2 = [v2 + v4["damage"]][0];
    if (!v4["damageOnlyWithOneProj"]) { pc = 846; continue; }
    pc = 775; continue;
  }
    case 775: {
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
    pc = 831; continue;
  }
    case 831: {
    const [t3, t4] = ofStep();
    if (!t4) { pc = 811; continue; }
    pc = 835; continue;
  }
    case 835: {
    pc = 846; continue;
  }
    case 839: {
    const [t5, t6] = inStep();
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
    const [t7, t8] = inStep();
    if (!t8) { pc = 83; continue; }
    pc = 853; continue;
  }
    case 853: {
    return v2;
  }
    }
  }
}
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
    if (![!capturedValue0["isIndirect"]][0]) { pc = 313; continue; }
    pc = 302; continue;
  }
    case 302: {
    pc = 313; continue;
  }
    case 313: {
    v1 = capturedValue3.deferProjectileTicks(capturedValue0["range"], capturedValue0["speed"], capturedValue0["position"], capturedValue0["startAngle"], !capturedValue0["passesEnvironment"]);
    pc = 330; continue;
  }
    case 330: {
    return;
  }
    }
  }
}
function getProjectileTicks(arg0, arg1, arg2, arg3, arg4, {mathBindings, dodgePlanner}) {
  let radians, v0, v10, v11, v12, v13, v3, v4, v6, v7, v8, v9;
  const stack = [];
  let pc = 0;
  while (true) {
    switch (pc) {
    case 0: {
    v0 = {0: arg2};
    arg2 = arg2;
    arg3 = arg3;
    v3 = arg1 / 20;
    v4 = Math.ceil(arg0 / v3);
    radians = mathBindings["MathUtil"].getRadians(arg3);
    v6 = Math.cos(radians);
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
    v9 = Math.min(v3, arg0 - v8 - 1 * v3);
    v10 = arg2[0] + v6 * v9;
    v11 = arg2[1] + v7 * v9;
    v12 = [v10, v11];
    if (!arg4) { pc = 270; continue; }
    pc = 229; continue;
  }
    case 229: {
    v13 = dodgePlanner.checkWallCollisions(arg2, v12);
    if (!v13) { pc = 270; continue; }
    pc = 257; continue;
  }
    case 257: {
    pc = 300; continue;
  }
    case 270: {
    arg2 = v12;
    v8 = numeric(v8) + (typeof numeric(v8) === 'bigint' ? 1n : 1);
    pc = 127; continue;
  }
    case 300: {
    return v0;
  }
    }
  }
}
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
    throw Reflect.construct(Error, [], Error);
  }
    case 267: {
    v3 = Reflect.construct(DataView, [v2], DataView);
    if (!arg0) { pc = 342; continue; }
    pc = 283; continue;
  }
    case 283: {
    return [v3.getInt32(0, !![]), v3.getInt32(4, !![]), v3.getInt32(8, !![])];
  }
    case 342: {
    return [v3.getInt32(0, !![]), v3.getInt32(4, !![])];
  }
    case 382: {
    if (!arg0) { pc = 422; continue; }
    pc = 385; continue;
  }
    case 385: {
    return [v4.getX(), v4.getY(), v4.getZ()];
  }
    case 422: {
    return [v4.getX(), v4.getY()];
  }
    }
  }
}
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
    v4 = closure1;
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
    v5 = [v5 + 1][0];
    pc = 134; continue;
  }
    case 189: {
    return v3;
  }
    }
  }
}
function getDamageOnlyWithOneProj() {
  let v0;
  function hasResult__8402448478f7e9dd(input = {}) {
  v0 = this;
  const localValue01 = v0;
  const completed2 = localValue01["completed"];
  const result3 = completed2 !== null;
  return result3;
  }
  const metadata39 = {"id":"f_8b1613f8ca3533fa","bindings":["getDamageOnlyWithOneProj"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getMaxRange({capturedValue0}) {
  let v0;
  function getConstantFlyTime__72e19c8c0bb67b6a(input = {}) {
  v0 = this;
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
  const metadata32 = {"id":"f_7360abf0f0b60b37","bindings":["getMaxRange"],"argumentCount":1,"variableCount":3,"constants":[],"closures":[{"flags":13,"variableIndex":1174,"atom":0,"name":""}]};
}
function getConstantFlyTime({capturedValue0}) {
  let v0;
  function getPassesEnvironment__6dace8431da70dd2(input = {}) {
  v0 = this;
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
  }
  }
  const metadata31 = {"id":"f_72e19c8c0bb67b6a","bindings":["getConstantFlyTime"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getCollisionRadius({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicCharacterData5 = capturedValue04["LogicCharacterData"];
  const CollisionRadius6 = LogicCharacterData5["CollisionRadius"];
  const callResult7 = add3.call(instance2, CollisionRadius6);
  const readInt8 = callResult7["readInt"];
  const callResult9 = readInt8.call(callResult7);
  return callResult9;
}
function getBoomerangType(arg0, {capturedValue0, capturedValue1}) {
  function function_a6c866cd__a6c866cdf0918a97(input = {}) {
  const capturedValue01 = capturedValue0;
  const failed2 = capturedValue01["failed"];
  const capturedValue13 = capturedValue1;
  const argument04 = arg0;
  const callResult5 = Reflect.apply(failed2, capturedValue01, [capturedValue13, argument04]);
  return callResult5;
  }
  const metadata50 = {"id":"f_acf3b3f1e9e6ef6a","bindings":["getBoomerangType"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getMirrored(arg0, arg1) {
  let v0, v1, v2, v4;
  function getDynamicDamage__9f7f4eb6bb728899(input = {}) {
  v4 = this;
  const BattleCache1 = BattleCache;
  const TickPosition2 = BattleCache1["TickPosition"];
  if (TickPosition2) {
  v0 = TickPosition2;
  const BattleCache3 = BattleCache;
  const CurrentSpeed4 = BattleCache3["CurrentSpeed"];
  v1 = CurrentSpeed4;
  const localValue15 = v1;
  const result6 = localValue15 / 20;
  v2 = result6;
  const localValue47 = v4;
  const getPathByAngle8 = localValue47["getPathByAngle"];
  const localValue09 = v0;
  const argument010 = arg0;
  const localValue211 = v2;
  const callResult12 = Reflect.apply(getPathByAngle8, localValue47, [localValue09, argument010, localValue211]);
  pathByAngle = callResult12;
  const localValue413 = v4;
  const getPathDamage14 = localValue413["getPathDamage"];
  const pathByAngle15 = pathByAngle;
  const argument116 = arg1;
  const callResult17 = Reflect.apply(getPathDamage14, localValue413, [pathByAngle15, argument116]);
  return callResult17;
  } else {
  const BattleCache18 = BattleCache;
  const ServerPosition19 = BattleCache18["ServerPosition"];
  v0 = ServerPosition19;
  const BattleCache20 = BattleCache;
  const CurrentSpeed21 = BattleCache20["CurrentSpeed"];
  v1 = CurrentSpeed21;
  const localValue122 = v1;
  const result23 = localValue122 / 20;
  v2 = result23;
  const localValue424 = v4;
  const getPathByAngle25 = localValue424["getPathByAngle"];
  const localValue026 = v0;
  const argument027 = arg0;
  const localValue228 = v2;
  const callResult29 = Reflect.apply(getPathByAngle25, localValue424, [localValue026, argument027, localValue228]);
  pathByAngle = callResult29;
  const localValue430 = v4;
  const getPathDamage31 = localValue430["getPathDamage"];
  const pathByAngle32 = pathByAngle;
  const argument133 = arg1;
  const callResult34 = Reflect.apply(getPathDamage31, localValue430, [pathByAngle32, argument133]);
  return callResult34;
  }
  }
  const metadata48 = {"id":"f_a45a34d7c4b6bd89","bindings":["getMirrored"],"argumentCount":0,"variableCount":0,"constants":[],"closures":[{"flags":13,"variableIndex":8,"atom":0,"name":""}]};
}
function isBouncing() {
  let v2;
  function isIndirect__447fe9a2bdd36ab1(input = {}) {
  v2 = this;
  const localValue21 = v2;
  const getChargeType2 = localValue21["getChargeType"];
  const callResult3 = Reflect.apply(getChargeType2, localValue21, []);
  chargeType = callResult3;
  const localValue24 = v2;
  const getProjectile5 = localValue24["getProjectile"];
  const callResult6 = Reflect.apply(getProjectile5, localValue24, []);
  projectile = callResult6;
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
  }
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
  }
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
  }
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
  }
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
  }
  }
  }
  }
  }
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
  }
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
  }
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
  }
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
  }
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
  }
  }
  }
  }
  }
  } else {
  const arrayValue95 = [];
  const result96 = !arrayValue95;
  const result97 = !result96;
  return result97;
  }
  }
  }
  const metadata19 = {"id":"f_467cc9b1b326e089","bindings":["isBouncing"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function isIndirect(arg0) {
  let v0;
  function findStoredTouch__422d016251b7bfd4(input = {}) {
  v0 = this;
  const localValue01 = v0;
  const touches2 = localValue01["touches"];
  const find3 = touches2["find"];
  const callback4 = frame.makeClosure(0, {args, locals, refs, frame});
  const callResult5 = Reflect.apply(find3, touches2, [callback4]);
  return callResult5;
  }
  const metadata18 = {"id":"f_447fe9a2bdd36ab1","bindings":["isIndirect"],"argumentCount":0,"variableCount":3,"constants":[],"closures":[]};
}
function getIndirect(arg0, {capturedValue0}) {
  function function_585f09dc__585f09dc40aa32b7_1(input = {}) {
  const capturedValue01 = capturedValue0;
  const report2 = capturedValue01["report"];
  const argument03 = arg0;
  const callResult4 = Reflect.apply(report2, capturedValue01, [argument03]);
  return callResult4;
  }
  const metadata26 = {"id":"f_614284f07145d36c","bindings":["getIndirect"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getPassesEnvironment({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const PiercesEnvironment6 = LogicProjectileData5["PiercesEnvironment"];
  const callResult7 = add3.call(instance2, PiercesEnvironment6);
  const readU88 = callResult7["readU8"];
  const callResult9 = readU88.call(callResult7);
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
  const callResult18 = add14.call(instance13, PiercesEnvironmentLikeButter17);
  const readU819 = callResult18["readU8"];
  const callResult20 = readU819.call(callResult18);
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
  const callResult29 = add25.call(instance24, PassesEnvironment28);
  const readU830 = callResult29["readU8"];
  const callResult31 = readU830.call(callResult29);
  const result32 = !callResult31;
  const result33 = !result32;
  return result33;
  }
  }
}
function getSpawnAreaEffectObject() {
  let v1;
  function isCharacterPositionBased__7979a8197aa93304(input = {}) {
  v1 = this;
  const localValue11 = v1;
  const getBaseName2 = localValue11["getBaseName"];
  const callResult3 = Reflect.apply(getBaseName2, localValue11, []);
  baseName = callResult3;
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
  }
  }
  const metadata35 = {"id":"f_7ade4e006c02f6fc","bindings":["getSpawnAreaEffectObject"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":9,"variableIndex":31,"atom":0,"name":""},{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getSteeringLifeTime({capturedValue0}) {
  let v0;
  const localValue01 = v0;
  const instance2 = localValue01["instance"];
  const add3 = instance2["add"];
  const capturedValue04 = capturedValue0;
  const LogicProjectileData5 = capturedValue04["LogicProjectileData"];
  const SteeringLifeTime6 = LogicProjectileData5["SteeringLifeTime"];
  const callResult7 = add3.call(instance2, SteeringLifeTime6);
  const readInt8 = callResult7["readInt"];
  const callResult9 = readInt8.call(callResult7);
  return callResult9;
}
function getTotalDelta({capturedValue0}) {
  let v0;
  function isDoNotRotateClip__e5ae9c59d3377742(input = {}) {
  v0 = this;
  const localValue01 = v0;
  const getRendering2 = localValue01["getRendering"];
  const callResult3 = Reflect.apply(getRendering2, localValue01, []);
  const capturedValue04 = capturedValue0;
  const DO_NOT_ROTATE_THE_CLIP5 = capturedValue04["DO_NOT_ROTATE_THE_CLIP"];
  const result6 = callResult3 === DO_NOT_ROTATE_THE_CLIP5;
  return result6;
  }
  const metadata61 = {"id":"f_e5e993a2b087b379","bindings":["getTotalDelta"],"argumentCount":0,"variableCount":1,"constants":[],"closures":[{"flags":13,"variableIndex":7,"atom":0,"name":""}]};
}
function getAngleDifference(arg0, arg1) {
  let v0;
  const Math1 = Math;
  const abs2 = Math1["abs"];
  const argument03 = arg0;
  const argument14 = arg1;
  const result5 = argument03 - argument14;
  const result6 = result5 % 360;
  const callResult7 = abs2.call(Math1, result6);
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
}
function getRadians(arg0) {
  const argument01 = arg0;
  const Math2 = Math;
  const PI3 = Math2["PI"];
  const result4 = PI3 / 180;
  const result5 = argument01 * result4;
  return result5;
}
function getDegreesToPoint(arg0, {captured_getInstance}) {
  let v0, v4;
  function isOwn__4933eeb947b62798(input = {}) {
  const argument01 = arg0;
  const result2 = argument01 < 0;
  if (!result2) {
  const captured_getInstance3 = captured_getInstance;
  const callResult4 = Reflect.construct(captured_getInstance3, [], captured_getInstance3);
  v0 = callResult4;
  const localValue05 = v0;
  const getLogicPlayer6 = localValue05["getLogicPlayer"];
  const argument07 = arg0;
  const callResult8 = Reflect.apply(getLogicPlayer6, localValue05, [argument07]);
  logicPlayer = callResult8;
  const localValue09 = v0;
  const getOwnPlayerIndex10 = localValue09["getOwnPlayerIndex"];
  const callResult11 = Reflect.apply(getOwnPlayerIndex10, localValue09, []);
  ownPlayerIndex = callResult11;
  const logicPlayer12 = logicPlayer;
  const getPlayerIndex13 = logicPlayer12["getPlayerIndex"];
  const callResult14 = Reflect.apply(getPlayerIndex13, logicPlayer12, []);
  playerIndex = callResult14;
  const ownPlayerIndex15 = ownPlayerIndex;
  const playerIndex16 = playerIndex;
  const result17 = ownPlayerIndex15 == playerIndex16;
  v4 = result17;
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
  }
  } else {
  const arrayValue25 = [];
  const result26 = !arrayValue25;
  return result26;
  }
  }
  const metadata21 = {"id":"f_4e4c4943cd1b3f84","bindings":["getDegreesToPoint"],"argumentCount":4,"variableCount":1,"constants":[],"closures":[]};
}
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
  const callResult16 = sqrt8.call(Math7, result15);
  return callResult16;
}
function checkSweptCollision({capturedValue0, capturedValue1}) {
  let v0;
  function getSpawnAreaEffectObject__7ade4e006c02f6fc(input = {}) {
  v0 = this;
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
  const metadata36 = {"id":"f_7b14b1912d56a4ae","bindings":["checkSweptCollision"],"argumentCount":5,"variableCount":11,"constants":[],"closures":[]};
}
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
  const callResult12 = getPathByAngle8.call(localValue47, localValue09, argument010, localValue211);
  const localValue413 = v4;
  const getPathDamage14 = localValue413["getPathDamage"];
  const pathByAngle15 = pathByAngle;
  const argument116 = arg1;
  const callResult17 = getPathDamage14.call(localValue413, pathByAngle15, argument116);
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
  const callResult29 = getPathByAngle25.call(localValue424, localValue026, argument027, localValue228);
  const localValue430 = v4;
  const getPathDamage31 = localValue430["getPathDamage"];
  const pathByAngle32 = pathByAngle;
  const argument133 = arg1;
  const callResult34 = getPathDamage31.call(localValue430, pathByAngle32, argument133);
  return callResult34;
  }
}
function hasResult() {
  let v0;
  const localValue01 = v0;
  const completed2 = localValue01["completed"];
  const result3 = completed2 !== null;
  return result3;
}
function isCharacterPositionBased(arg0, {captured_getInstance, captured_45_FRIENDLY_FIRE}) {
  let v0, v4;
  function isOwnTeam__783de06f8ce503f7(input = {}) {
  const argument01 = arg0;
  const result2 = argument01 < 0;
  if (!result2) {
  const captured_getInstance3 = captured_getInstance;
  const callResult4 = Reflect.construct(captured_getInstance3, [], captured_getInstance3);
  v0 = callResult4;
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
  logicPlayer = callResult13;
  const localValue014 = v0;
  const getOwnPlayerTeam15 = localValue014["getOwnPlayerTeam"];
  const callResult16 = Reflect.apply(getOwnPlayerTeam15, localValue014, []);
  ownPlayerTeam = callResult16;
  const logicPlayer17 = logicPlayer;
  const getPlayerTeam18 = logicPlayer17["getPlayerTeam"];
  const callResult19 = Reflect.apply(getPlayerTeam18, logicPlayer17, []);
  playerTeam = callResult19;
  const ownPlayerTeam20 = ownPlayerTeam;
  const playerTeam21 = playerTeam;
  const result22 = ownPlayerTeam20 == playerTeam21;
  v4 = result22;
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
  }
  } else {
  const arrayValue30 = [];
  const result31 = !arrayValue30;
  return result31;
  }
  } else {
  const arrayValue32 = [];
  const result33 = !arrayValue32;
  return result33;
  }
  }
  const metadata34 = {"id":"f_7979a8197aa93304","bindings":["isCharacterPositionBased"],"argumentCount":0,"variableCount":2,"constants":[],"closures":[]};
}
function f64Sqrt(arg0, {capturedValue0, capturedValue1}) {
  let v0, v1;
  const capturedValue01 = capturedValue0;
  const get2 = capturedValue01["get"];
  const localValue13 = v1;
  const callResult4 = get2.call(capturedValue01, localValue13);
  const localValue05 = v0;
  if (localValue05) {
  const localValue06 = v0;
  const localValue17 = v1;
  const argument08 = arg0;
  const callResult9 = localValue06.call(undefined, localValue17, argument08);
  return callResult9;
  } else {
  const capturedValue010 = capturedValue0;
  const set11 = capturedValue010["set"];
  const localValue112 = v1;
  const capturedValue113 = capturedValue1;
  const localValue114 = v1;
  const ORDER15 = localValue114["ORDER"];
  const callResult16 = capturedValue113.call(undefined, ORDER15);
  const callResult17 = set11.call(capturedValue010, localValue112, callResult16);
  const localValue018 = v0;
  const localValue119 = v1;
  const argument020 = arg0;
  const callResult21 = localValue018.call(undefined, localValue119, argument020);
  return callResult21;
  }
}
