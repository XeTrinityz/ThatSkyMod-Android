# -*- coding: utf-8 -*-
"""
IDA Pro 9 Script: Generate Signature Scans from TSM_Offsets.h (ARM64 Version)
This script extracts bytes at each offset and creates signature patterns for ARM64 binaries.
Output: signatures.json containing all signatures
"""

import ida_bytes
import ida_segment
import ida_funcs
import ida_name
import idautils
import idc
import json
import re

# All offsets from offsets.h (ARM64)
OFFSETS = {
    # Core game offsets
    "Game": 0x2A20D50,
    "AudienceBarn": 0x27C0CD8,
    
    # Function offsets
    "kLuaDebugDoString": 0x21344B8,
    "kLuaPushLightUserData": 0xB0A5BC,
    "kWingBuffUpdate": 0x20BD83C,
    "kRadianceBarn": 0x1195B34,
    "kAccountServerSetSession": 0xC73144,
    "kHttpClientSetUserAgent": 0x2121360,
    "kShouldDisplay": 0xE6ABB8,
    "kCameraSystemInitialize": 0x104CFC8,
    
    # Patch offsets
    "kInvincibility": 0x27C8ED8,
    "kAutoCharge": 0x27C8ED4,
    "kDisableRainDrain": 0x27C8EE8,
    "kDarkCreatureTame": 0x27F98B4,
    "kAllowAfk": 0x263DCA0,
    "kRunSpeed": 0x26A099C,
    "kSuperSlidey": 0x2AF0DC4,
    "kAutoCollectAllFragments": 0x2ADB290,
    "kAutoFragmentWarp": 0x27FCBC4,
    "kFastBurn": 0x27FA4E8,
    "kDyeDebug": 0x28082A0,
    "kEnableAllRelationshipAbilities": 0x2ABB12C,
    "kFakeCapeLevelEnabled": 0x2ADF400,
    "kFakeCapeLevel": 0x26A0FB4,
    "kAllowOverride": 0x2753978,
    "kSunMoonXPosition": 0x2753988,
    "kSunMoonYPosition": 0x25235B4,
    "kSunMoon": 0x275398C,
    "kSunMoonSize": 0x25235B8,
    "kMoonPhase": 0x2753990,
    "kExposure": 0x25235C0,
    "kFlameToCandleScale": 0x264E0D4,
    "kFlowerHeight": 0x2665B90,
    "kFlowerSize": 0x2665B94,
    "kEnableGameCamSnap": 0x26B320C,
    "kAvatarCharcoaling": 0x27C6F28,
    "kForceEthereal": 0x27C6F14,
    "kAllNpcsHaveRadar": 0x27C6F2E,
    "kRevealPlayers": 0xECF968,
    "kEnableMultiplayer": 0x267A710,
    "kDisableGates": 0x27F22F4,
    "kFastHome": 0x1926D54,
    "kFreezeKrills": 0x117DEF8,
    "kBirthdayKrills": 0x1180CE0,
    "kHideHudExceptForStarFragments": 0x2ADB294,
    "kTguiPauseAnimation": 0x2AEBD14,
    "kUiShowHierarchy": 0x2AEBD1C,
    "kDebugShowSpiritLocations": 0x27DDB20,
    "kShowRadarForPreviousWingBuffs": 0x2B5DE10,
    "kEnableShrineRadar": 0x2B149A0,
    "kShowAllFeedback": 0x2B149A4,
    "kDebugLevelLinks": 0x2C7B160,
    "kFishSchoolDebug": 0x2817510,
    "kTvDebugUi": 0x2AE7CB0,
    "kDisableWindWall": 0x12CA2FC,
    "kDisableLevelChangeEvents": 0x27E46D4,
    "kDisableTerrain": 0x27EF6E9,
    "kDisableObjects": 0x27EF6ED,
    "kDisableObjectSkirts": 0x27EF6EE,
    "kDisableModels": 0x27EF6EF,
    "kDisableAvatars": 0x27EF6EC,
    "kEnableGravity": 0x26A095C,
    "kEnableClouds": 0x2650068,
    "kEnableWater": 0x26800EC,
    "kEnableOcean": 0x26800F0,
    "kDisableLights": 0x2A5E7C4,
    "kAutoCompleteQuests": 0x1A92848,
    "kAutoBurnDyes": 0x1197034,
    "kAutoBurnDyes1": 0x1196F48,
    "kSuperLaunch": 0x2ADF404,
    "kSpellEmitter": 0xE83E40,
    "kScooterMode": 0xEE058C,
    "kRainbowGlow": 0x2AB7268,
    "kRainbowTrails": 0x27CBFB8,
    "kBubbleTrails": 0x27CBFB4,
    "kUnlockTrust": 0x2AF48C8,
    "kAutoBurnPlant1": 0x1197034,
    "kAutoBurnPlant2": 0x1196FBC,
    "kAutoBurnPlant3": 0x1196F48,
}

def get_image_base():
    """Get the image base address"""
    return idaapi.get_imagebase()

def convert_to_rva(offset):
    """Convert offset to RVA by subtracting image base"""
    image_base = get_image_base()
    return image_base + offset

def is_in_code_section(ea):
    """Check if address is in a code section"""
    seg = ida_segment.getseg(ea)
    if seg:
        return (seg.perm & ida_segment.SEGPERM_EXEC) != 0
    return False

def is_in_data_section(ea):
    """Check if address is in a data section"""
    seg = ida_segment.getseg(ea)
    if seg:
        seg_name = ida_segment.get_segm_name(seg)
        return '.data' in seg_name or '.rdata' in seg_name or '.bss' in seg_name
    return False

def get_signature_pattern(ea, length=32, wildcard_refs=True):
    """
    Generate a signature pattern from the given address
    
    Args:
        ea: Effective address
        length: Length of pattern to extract
        wildcard_refs: If True, wildcard immediate values and addresses
    
    Returns:
        tuple: (pattern_string, sig_type, offset_within_pattern, xref_insn_ea)
    """
    pattern = []
    
    # For data sections, we want to capture the reference, not the data itself
    if is_in_data_section(ea):
        # Look for xrefs to this data
        xrefs = list(idautils.XrefsTo(ea))
        if xrefs:
            # Collect all code references
            code_xrefs = []
            for xref in xrefs:
                if is_in_code_section(xref.frm):
                    code_xrefs.append(xref.frm)
            
            # Try to use the first code reference
            if code_xrefs:
                insn_ea = code_xrefs[0]
                insn_start = idc.get_item_head(insn_ea)
                # Generate pattern centered on the xref instruction
                pattern_str, sig_type, offset = generate_code_signature_data_xref(insn_start)
                
                # Store all xrefs as fallback options
                return pattern_str, "data_xref", offset, insn_start
        
        # If no code refs, treat as raw data
        for i in range(min(length, 16)):
            byte = ida_bytes.get_byte(ea + i)
            if byte is not None:
                pattern.append(f"{byte:02X}")
            else:
                pattern.append("??")
        
        return " ".join(pattern), "data", 0, None
    
    # For code sections, generate instruction-based signature
    # Just use the address itself - function start may change too much
    pattern_str, sig_type, offset = generate_code_signature(ea, ea)
    return pattern_str, sig_type, offset, None

def should_wildcard_arm64_insn(insn, insn_bytes):
    """
    Determine which bytes should be wildcarded in an ARM64 instruction.
    ARM64 instructions are 4 bytes, we wildcard based on instruction type.
    
    Returns:
        list: [bool, bool, bool, bool] indicating which bytes to wildcard
    """
    mnem = idc.print_insn_mnem(insn.ea).lower()
    
    # For instructions with immediate values or addresses, wildcard the immediate bytes
    # ARM64 encoding: immediates are encoded in specific bit fields
    
    # Instructions that load addresses/data (ADRP, ADR, LDR with literal)
    if mnem in ['adrp', 'adr']:
        # Wildcard bytes 1-3 (immediate is in bits 5-23 and 29-30)
        return [False, True, True, True]
    
    # LDR with immediate or literal
    if mnem.startswith('ldr') or mnem.startswith('str'):
        # Check if it has an immediate operand
        for op in insn.ops:
            if op.type in [idaapi.o_imm, idaapi.o_mem]:
                # Wildcard bytes 1-2 (immediate offset)
                return [False, True, True, False]
        return [False, False, False, False]
    
    # Branch instructions (B, BL, etc)
    if mnem in ['b', 'bl', 'cbz', 'cbnz', 'tbz', 'tbnz']:
        # Wildcard offset bytes (varies by instruction)
        return [False, True, True, True]
    
    # MOV with immediate
    if mnem.startswith('mov') and any(op.type == idaapi.o_imm for op in insn.ops):
        return [False, True, True, False]
    
    # ADD/SUB with immediate
    if mnem in ['add', 'sub', 'adds', 'subs']:
        for op in insn.ops:
            if op.type == idaapi.o_imm:
                return [False, True, False, False]
        return [False, False, False, False]
    
    # Default: don't wildcard for ALU and other instructions
    return [False, False, False, False]

def generate_code_signature_data_xref(xref_ea):
    """
    Generate signature for data xref - ARM64 version
    
    Args:
        xref_ea: Address of instruction that references the data
    
    Returns:
        tuple: (pattern_string, signature_type, offset_within_pattern)
    """
    sig_bytes = []
    sig_mask = []  # True = wildcard, False = exact
    
    current_ea = xref_ea
    instructions_collected = 0
    max_instructions = 3  # ARM64: 3 instructions = 12 bytes
    
    while instructions_collected < max_instructions:
        insn = idautils.DecodeInstruction(current_ea)
        if not insn:
            break
        
        # ARM64 instructions are always 4 bytes
        insn_bytes = ida_bytes.get_bytes(current_ea, 4)
        if not insn_bytes or len(insn_bytes) != 4:
            break
        
        # Determine which bytes to wildcard
        wildcard_mask = should_wildcard_arm64_insn(insn, insn_bytes)
        
        # Add bytes and mask
        for i in range(4):
            sig_bytes.append(insn_bytes[i])
            sig_mask.append(wildcard_mask[i])
        
        current_ea += 4
        instructions_collected += 1
    
    # Trim wildcards from front and back
    while sig_mask and sig_mask[-1]:
        sig_bytes.pop()
        sig_mask.pop()
    
    while sig_mask and sig_mask[0]:
        sig_bytes.pop(0)
        sig_mask.pop(0)
    
    # Build IDA-style signature
    pattern = []
    for byte, is_wild in zip(sig_bytes, sig_mask):
        if is_wild:
            pattern.append("??")
        else:
            pattern.append(f"{byte:02X}")
    
    return " ".join(pattern), "data_xref", 0

def build_pattern_from_insns(insns, start_ea):
    """Build a pattern string from instruction list - ARM64 version"""
    wildcard_bytes = set()
    
    # Process each instruction (ARM64: each is 4 bytes)
    for insn_ea, insn in insns:
        # Get instruction bytes
        insn_bytes = ida_bytes.get_bytes(insn_ea, 4)
        if not insn_bytes:
            continue
        
        # Determine which bytes to wildcard
        wildcard_mask = should_wildcard_arm64_insn(insn, insn_bytes)
        
        # Mark bytes to wildcard
        for j in range(4):
            if wildcard_mask[j]:
                wildcard_bytes.add(insn_ea + j)
    
    # Build pattern
    pattern = []
    search_start = insns[0][0]
    total_size = len(insns) * 4  # ARM64: 4 bytes per instruction
    
    for i in range(total_size):
        byte_ea = search_start + i
        if byte_ea in wildcard_bytes:
            pattern.append("??")
        else:
            byte = ida_bytes.get_byte(byte_ea)
            pattern.append(f"{byte:02X}")
    
    return " ".join(pattern)

def is_pattern_unique(pattern, target_ea):
    """Check if pattern matches only one location (the target)"""
    try:
        # Use IDA's binary search
        ea_min = ida_segment.getseg(target_ea).start_ea
        ea_max = ida_segment.getseg(target_ea).end_ea
        
        matches = []
        addr = ea_min - 1
        
        # Find all matches (limit to 3 to save time)
        for _ in range(3):
            addr = ida_bytes.bin_search(addr + 1, ea_max, pattern, 16)
            if addr == idaapi.BADADDR:
                break
            matches.append(addr)
        
        # Unique if exactly 1 match at our target
        return len(matches) == 1 and matches[0] == target_ea
    except:
        return False

def generate_code_signature_short(start_ea, target_ea):
    """
    Generate a shorter, more exact signature (less wildcards) as fallback - ARM64 version
    """
    pattern = []
    offset = 0
    # ARM64: align to instruction boundaries (4 bytes)
    instructions_before = 3
    instructions_after = 3
    
    # Align start_ea to instruction boundary
    aligned_start = (start_ea // 4) * 4
    search_start = max(aligned_start - (instructions_before * 4), 
                      ida_segment.getseg(start_ea).start_ea if ida_segment.getseg(start_ea) else aligned_start)
    
    pattern_length = (instructions_before + instructions_after) * 4
    
    for i in range(pattern_length):
        current_ea = search_start + i
        
        seg = ida_segment.getseg(current_ea)
        if not seg or current_ea >= seg.end_ea:
            break
        
        byte = ida_bytes.get_byte(current_ea)
        pattern.append(f"{byte:02X}")
        
        if current_ea == target_ea:
            offset = i
    
    return " ".join(pattern), "code_short", offset

def generate_code_signature(start_ea, target_ea):
    """
    Generate signature for code section - ARM64 version: grow until unique
    
    Args:
        start_ea: Where to start the signature
        target_ea: The target address we're trying to find
    
    Returns:
        tuple: (pattern_string, signature_type, offset_within_pattern)
    """
    # Align to ARM64 instruction boundary
    start_ea = (start_ea // 4) * 4
    target_ea = (target_ea // 4) * 4
    
    all_insns = []
    temp_ea = start_ea
    max_instructions = 15  # Safety limit
    
    # Build instruction list starting from target
    for _ in range(max_instructions):
        insn = idautils.DecodeInstruction(temp_ea)
        if not insn:
            break
        all_insns.append((temp_ea, insn))
        temp_ea = temp_ea + 4  # ARM64: fixed 4-byte instructions
        
        # Try to make signature from what we have so far
        if len(all_insns) >= 3:  # Need at least 3 instructions
            test_pattern = build_pattern_from_insns(all_insns, start_ea)
            
            # Test uniqueness by searching
            if test_pattern and is_pattern_unique(test_pattern, start_ea):
                # Found unique signature!
                break
    
    if not all_insns:
        # Fallback to simple byte pattern
        return generate_code_signature_short(start_ea, target_ea)
    
    # Generate final pattern
    final_pattern = build_pattern_from_insns(all_insns, start_ea)
    
    # Calculate offset to target (ARM64: 4 bytes per instruction)
    offset_bytes = 0
    search_start = all_insns[0][0]
    for insn_ea, insn in all_insns:
        if insn_ea == target_ea:
            break
        offset_bytes += 4
    
    return final_pattern, "code", offset_bytes

def generate_all_signatures():
    """Generate signatures for all offsets"""
    signatures = {}
    
    # Get image base
    image_base = idaapi.get_imagebase()
    
    print(f"Image Base: 0x{image_base:X}")
    print(f"Generating signatures for {len(OFFSETS)} offsets...\n")
    
    failed_offsets = []
    
    for name, offset in OFFSETS.items():
        ea = convert_to_rva(offset)
        
        print(f"Processing {name} (0x{offset:X} -> 0x{ea:X})...")
        
        # Verify address is valid
        if not ida_bytes.is_loaded(ea):
            print(f"  [!] Address not loaded, adding placeholder")
            failed_offsets.append(name)
            signatures[name] = {
                "offset": offset,
                "pattern": "",
                "type": "failed",
                "pattern_offset": 0,
                "segment": "unknown",
                "is_code": False,
                "is_data": False,
                "error": "Address not loaded"
            }
            continue
        
        # Get segment info
        seg = ida_segment.getseg(ea)
        if not seg:
            print(f"  [!] Not in valid segment, adding placeholder")
            failed_offsets.append(name)
            signatures[name] = {
                "offset": offset,
                "pattern": "",
                "type": "failed",
                "pattern_offset": 0,
                "segment": "unknown",
                "is_code": False,
                "is_data": False,
                "error": "Not in valid segment"
            }
            continue
        
        seg_name = ida_segment.get_segm_name(seg)
        is_code = is_in_code_section(ea)
        is_data = is_in_data_section(ea)
        
        print(f"  Segment: {seg_name}, Code: {is_code}, Data: {is_data}")
        
        # Generate signature
        try:
            pattern, sig_type, pattern_offset, xref_insn = get_signature_pattern(ea)
            
            sig_data = {
                "offset": offset,
                "pattern": pattern,
                "type": sig_type,
                "pattern_offset": pattern_offset,
                "segment": seg_name,
                "is_code": is_code,
                "is_data": is_data
            }
            
            # For data_xref, store the original data address for verification
            if sig_type == "data_xref" and xref_insn:
                sig_data["data_address"] = ea
                sig_data["xref_insn"] = xref_insn
            
            # Generate an alternative shorter signature as fallback
            if is_code and sig_type not in ["data_xref"]:
                try:
                    alt_pattern, alt_type, alt_offset = generate_code_signature_short(ea, ea)
                    sig_data["alt_pattern"] = alt_pattern
                    sig_data["alt_offset"] = alt_offset
                except:
                    pass
            
            signatures[name] = sig_data
            
            print(f"  ✓ Pattern ({len(pattern.split())} bytes): {pattern[:90]}{'...' if len(pattern) > 90 else ''}")
            if sig_type == "data_xref":
                print(f"    (via xref at 0x{xref_insn:X})")
            if "alt_pattern" in sig_data:
                print(f"  + Alt pattern: {sig_data['alt_pattern'][:60]}...")
            
        except Exception as e:
            print(f"  [!] Failed to generate signature: {e}")
            failed_offsets.append(name)
            signatures[name] = {
                "offset": offset,
                "pattern": "",
                "type": "failed",
                "pattern_offset": 0,
                "segment": seg_name if seg else "unknown",
                "is_code": is_code if seg else False,
                "is_data": is_data if seg else False,
                "error": str(e)
            }
    
    print(f"\n{'='*80}")
    print(f"Generated {len(signatures)} signatures")
    if failed_offsets:
        print(f"Failed: {len(failed_offsets)} offsets")
        print(f"Failed offsets: {', '.join(failed_offsets)}")
    
    return signatures

def save_signatures(signatures, output_file="signatures.json"):
    """Save signatures to JSON file"""
    output_path = idc.get_idb_path().replace(".i64", f"_{output_file}").replace(".idb", f"_{output_file}")
    
    with open(output_path, 'w') as f:
        json.dump(signatures, f, indent=2)
    
    print(f"\n✓ Signatures saved to: {output_path}")
    return output_path

def main():
    print("="*80)
    print("TSM Signature Generator for IDA Pro 9 (ARM64)")
    print("="*80)
    print()
    
    signatures = generate_all_signatures()
    output_path = save_signatures(signatures)
    
    print("\n" + "="*80)
    print("DONE! Use ida_regenerate_offsets.py with the signatures file to rebuild the header.")
    print("="*80)

if __name__ == "__main__":
    main()
