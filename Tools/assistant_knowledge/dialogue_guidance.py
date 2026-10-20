"""Bounded, read-only clarification and follow-ups; no model interpolation."""
import re
try:
    from .workflow_answers import verified_workflow
except ImportError:
    from workflow_answers import verified_workflow

FOLLOWUP=re.compile(
 r"^(?:what(?:'s| is)? next|where (?:do i|should i) click next|"
 r"what (?:do i need|are the prerequisites)|before (?:i|we) start|"
 r"it (?:doesn't|does not) work|why (?:doesn't|does not) it work|"
 r"what about (?:one with )?gravity off|i (?:don't|do not) have (?:that|the prerequisites))"
 r"[?.! ]*$",re.I)

def is_followup(question):
    return bool(FOLLOWUP.fullmatch(question.strip()))

def scope_reply(question):
    if re.search(r"\b(?:whole|entire)\s+(?:game|world|level|scene)\b",question,re.I) or re.search(
        r"\b(?:automatically|autonomously)\b.{0,60}\b(?:create|generate|build)\b.{0,40}\b(?:game|world|level|scene)\b",
        question,re.I):
        return ("I can explain verified editor workflows and help with specific "
                "user-directed tasks. I cannot autonomously create a game or world. "
                "I won't guess at unverified capabilities. Which editor task would you like to work through?")
    return None

def clarification(question,repo,database):
    def checked(text,queries):
        recipes=[verified_workflow(q,repo,database) for q in queries]
        if not all(recipes):
            return "I cannot verify the source evidence for those workflow alternatives right now. Rebuild the knowledge index before relying on instructions."
        sources=list(dict.fromkeys(source for recipe in recipes
                    for source in re.findall(r"\[Source:[^\]\r\n]+\]",recipe)))
        return text+" "+" ".join(sources)
    q=question.lower()
    if re.search(r"\b(?:import|add)\b.{0,30}\bassets?\b",q) and not re.search(
            r"\b(?:model|static|character|rigged|animation|fbx|glb)\b",q):
        return checked("Are you importing a static model, a rigged Character, or an animation FBX?",
                       ["How do I import a static model?","How do I import a character?",
                        "How do I add animation FBX?"])
    if re.search(r"\b(?:add|assign|set up)\b.{0,35}\banimation\b",q) and not re.search(
            r"\b(?:external|fbx|clips?|gameplay|idle|walk|run|character import|fire|timing|weapon|arms)\b",q):
        return checked("Do you mean adding external FBX clips during Character import, "
                "assigning gameplay actions to clips, or setting a weapon's animation fire time?",
                ["How do I add animation FBX?","How do I assign gameplay actions to character clips?",
                 "How do I set fire time in animation?"])
    if re.search(r"\b(?:add|attach|set up)\b.{0,30}\bscript\b",q) and not re.search(
            r"\b(?:entity|object|lua|global|level|action)\b",q):
        return checked("Should this be an entity SCRIPT on a selected object, or a level-wide GLOBAL SCRIPT?",
                       ["How do I attach an entity script?","How do I add a global script?"])
    if re.search(r"\b(?:retarget|convert)\b",q) and re.search(r"\b(?:arms|arbitrary|any rig|different rigs|multi.rig)\b",q):
        return checked("The verified Character import path supports matching-rig animation "
                "transport or a usable single-humanoid mapping. I do not have a verified "
                "arbitrary FPS-arm rig conversion workflow. Describe the source and "
                "destination rigs so their actual compatibility can be checked.",
                ["How do I add animation FBX?"])
    return None

EXPLANATION=re.compile(
    r"^(?:then\s+)?what does (?:the classification|that|it) (?:actually )?(?:change|do|mean)[?.! ]*$",re.I)

def surface_explanation(question,messages,repo,database):
    # Resolve only explicit classification questions or a bounded explanatory
    # follow-up to the latest task. A casual intervening topic ends context.
    contextual=bool(EXPLANATION.fullmatch(question.strip()))
    topic=question
    if contextual:
        topic=""
        previous=[m for m in messages[:-1] if isinstance(m,dict) and
                  m.get("role")=="user" and isinstance(m.get("content"),str)]
        for message in reversed(previous[-8:]):
            if EXPLANATION.fullmatch(message["content"].strip()) or is_followup(message["content"]):
                continue
            topic=message["content"]
            break
    surface=bool(re.search(r"\bsurface type\b|\b(?:glass|impact) (?:impact )?classification\b",topic,re.I))
    explanatory=contextual or bool(re.search(
        r"\b(?:classification|shatter|destructible|breakage)\b",question,re.I))
    if not (surface and explanatory):
        return None
    recipe=verified_workflow("How do I set Surface Type?",repo,database)
    if not recipe:
        return "I cannot verify the Surface Type source evidence right now. I will not guess about impact or destruction behaviour."
    expected=recipe.split("**Expected result:**",1)[1].split("\n\n",1)[0].strip()
    troubleshooting=recipe.split("**Troubleshooting:**",1)[1].split("\n\n",1)[0].strip()
    sources=" ".join(re.findall(r"\[Source:[^\]\r\n]+\]",recipe))
    return expected+"\n\n"+troubleshooting+"\n\n"+sources

def followup_reply(question,messages,repo,database):
    explanation=surface_explanation(question,messages,repo,database)
    if explanation:
        return explanation
    if not is_followup(question):
        return None
    previous=[m for m in messages[:-1] if isinstance(m,dict) and
              m.get("role")=="user" and isinstance(m.get("content"),str)]
    recipe=None
    for m in reversed(previous[-8:]):
        if is_followup(m["content"]):
            continue
        recipe=verified_workflow(m["content"],repo,database)
        break  # Unrelated intervening conversation ends task context.
    if recipe is None:
        if previous and re.search(r"\b(?:renegade|studio|projectile|player|character|npc|script|equipment|import|animation)\b",
                                  previous[-1]["content"],re.I):
            return "I cannot verify a preceding editor workflow for that follow-up. Name the task or current control; I will not guess or use the model as editor authority."
        return None
    sources=" ".join(re.findall(r"\[Source:[^\]\r\n]+\]",recipe))
    title=recipe.split("\n",1)[0]
    q=question.lower()
    if q.startswith("where"):
        return ("I cannot see which step you have completed. Tell me the step "
                "number or the control you are looking at, and I can explain the "
                "next action from the verified guide. "+sources)
    if "gravity" in q:
        return ("Do you mean setting a travelling projectile's gravity to zero, "
                "or switching the weapon to Hitscan / instant ray? These are "
                "different settings; choose the one you intend. "+sources)
    if "work" in q:
        if "**Troubleshooting:**" in recipe:
            text=recipe.split("**Troubleshooting:**",1)[1].split("\n\n",1)[0]
            return "For this verified workflow, check:\n"+text.strip()+"\n\n"+sources
        return "Which step fails, and what message is shown? I cannot see your scene. "+sources
    if "next" in q:
        text=recipe.split("**Next:**",1)[1].split("\n\n",1)[0]
        return (title+"\n\n**Next stage:** "+text.strip()+
                "\n\nThis is the guide's next stage, not confirmation that your previous steps are complete.\n\n"+sources)
    text=recipe.split("**Before you start:**",1)[1].split("\n\n",1)[0]
    return "**Before you start:** "+text.strip()+"\n\n"+sources
