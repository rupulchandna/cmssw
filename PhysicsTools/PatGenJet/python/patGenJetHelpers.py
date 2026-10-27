"""Plain helper functions of the pat::GenJet python configuration (no module is defined here, nothing is added to a process)."""
import re
import FWCore.ParameterSet.Config as cms

STRIP = 'ForPatGenJets'  # postfix of every helper module label; the producers strip it

def userDataPSet(floats=(), ints=(), strip=(STRIP,)):
    """userData block of PATGenJetProducer with every PSet that PATUserDataHelper::fillDescription requires."""
    return cms.PSet(
        userFloats=cms.PSet(src=cms.VInputTag(*floats), labelPostfixesToStrip=cms.vstring(*strip)),
        userInts=cms.PSet(src=cms.VInputTag(*ints), labelPostfixesToStrip=cms.vstring(*strip)),
        userClasses=cms.PSet(src=cms.VInputTag()),
        userCands=cms.PSet(src=cms.VInputTag()),
        userFunctions=cms.vstring(),
        userFunctionLabels=cms.vstring(),
    )

_USER = re.compile(r"\b(userFloat|userInt)\(\s*'([^']*)'\s*\)")
_HAS = re.compile(r"\bhas(UserFloat|UserInt)\(\s*'([^']*)'\s*\)")

def expressionUserLabels(tables):
    """(floats, ints): every label read by userFloat('...')/userInt('...') in the Var expressions of the given tables
    (modules with a `variables` PSet) that is NOT guarded by hasUserFloat/hasUserInt of the same label in the same
    expression. Order of first appearance, no duplicates."""
    floats, ints = [], []
    for table in tables:
        variables = getattr(table, 'variables', None)
        if variables is None:
            continue
        for name in variables.parameterNames_():
            var = getattr(variables, name)
            if not hasattr(var, 'expr'):
                continue
            expr = var.expr.value()
            guarded = set(m.group(2) for m in _HAS.finditer(expr))
            for m in _USER.finditer(expr):
                label = m.group(2)
                lst = floats if m.group(1) == 'userFloat' else ints
                if label not in guarded and label not in lst:
                    lst.append(label)
    return floats, ints

# the three muon-subtraction floats are written with the jet's own pt, eta, phi; order matters
MUON_SUBTR = ('muonSubtrRawPt', 'muonSubtrRawEta', 'muonSubtrRawPhi')

def placeholderLabels(tables, providedFloats=(), providedInts=()):
    """Placeholder user data: the labels the table expressions read that no generator module provides.
    Returns dict(placeholderUserFloats=[...], placeholderUserInts=[...], placeholderPtEtaPhiUserFloats=[...])
    (the last one has 0 or 3 entries). Derived, not hard-coded (review M6)."""
    floats, ints = expressionUserLabels(tables)
    floats = [f for f in floats if f not in set(providedFloats)]
    ints = [i for i in ints if i not in set(providedInts)]
    ptetaphi = []
    if all(l in floats for l in MUON_SUBTR):
        ptetaphi = list(MUON_SUBTR)
        floats = [f for f in floats if f not in MUON_SUBTR]
    return dict(placeholderUserFloats=floats, placeholderUserInts=ints, placeholderPtEtaPhiUserFloats=ptetaphi)

