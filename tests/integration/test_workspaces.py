"""
Workspaces Integration Tests

Covers WorkspacesHandler and ProjectModel workspace logic:
 * CRUD (add / delete / rename / list / get)
 * customizeWorkspaces flag semantics (off = synthetic, on = persisted)
 * autoGenerate guard against clobbering a customised list
 * Widget ref add/remove
 * **Regression**: after deleting a group, widget refs in user-customised
   workspaces have their groupId and relativeIndex correctly shifted by
   the count of same-type widgets the deleted group contributed.

Copyright (C) 2020-2025 Alex Spataru
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-SerialStudio-Commercial
"""

import time

import pytest

from utils import APIError


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _list_workspaces(api_client):
    return api_client.command("project.workspaces.list")


def _get_workspace(api_client, wid):
    return api_client.command("project.workspaces.get", {"id": wid})


def _add_workspace(api_client, title="Workspace"):
    return api_client.command("project.workspaces.add", {"title": title})


def _delete_workspace(api_client, wid):
    return api_client.command("project.workspaces.delete", {"id": wid})


def _rename_workspace(api_client, wid, title):
    return api_client.command(
        "project.workspaces.rename", {"id": wid, "title": title}
    )


def _auto_generate(api_client):
    return api_client.command("project.workspaces.autoGenerate")


def _customize_get(api_client):
    return api_client.command("project.workspaces.customize.get")


def _customize_set(api_client, enabled):
    return api_client.command(
        "project.workspaces.customize.set", {"enabled": enabled}
    )


def _widget_add(api_client, workspace_id, widget_type, group_id, relative_index):
    return api_client.command(
        "project.workspaces.widgets.add",
        {
            "workspaceId": workspace_id,
            "widgetType": widget_type,
            "groupId": group_id,
            "relativeIndex": relative_index,
        },
    )


def _widget_remove(api_client, workspace_id, widget_type, group_id, relative_index):
    return api_client.command(
        "project.workspaces.widgets.remove",
        {
            "workspaceId": workspace_id,
            "widgetType": widget_type,
            "groupId": group_id,
            "relativeIndex": relative_index,
        },
    )


def _add_group_with_datasets(api_client, title, widget_type=0, dataset_count=2):
    """Create a group and add N datasets with plot enabled."""
    api_client.command("project.group.add", {"title": title, "widgetType": widget_type})
    time.sleep(0.15)

    for _ in range(dataset_count):
        # plotOption = 0x04 in SerialStudio::DatasetOption (graph)
        api_client.command("project.dataset.add", {"options": 0})
        time.sleep(0.08)


def _project_groups(api_client):
    return api_client.command("project.groups.list").get("groups", [])


# ---------------------------------------------------------------------------
# Customize flag + synthetic/automatic mode
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_fresh_project_has_customize_off(api_client, clean_state):
    """A new project starts with customizeWorkspaces=false."""
    result = _customize_get(api_client)
    assert result["enabled"] is False


@pytest.mark.project
def test_synthetic_workspaces_empty_for_empty_project(api_client, clean_state):
    """With zero groups, the synthetic auto-list is empty."""
    result = _list_workspaces(api_client)
    # Empty project: no groups → no synthetic workspaces
    assert result["count"] == 0
    assert result["customizeEnabled"] is False


@pytest.mark.project
def test_customize_set_toggles_flag(api_client, clean_state):
    """customize.set flips the flag and returns the new value."""
    _customize_set(api_client, True)
    assert _customize_get(api_client)["enabled"] is True

    _customize_set(api_client, False)
    assert _customize_get(api_client)["enabled"] is False


@pytest.mark.project
def test_customize_on_seeds_from_synthetic(api_client, clean_state):
    """
    Flipping customize on when a project has groups seeds m_workspaces
    from the synthetic list, so the user starts editing exactly what the
    automatic mode was showing.
    """
    # Build a project with two distinct groups so synthetic produces entries
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "B", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.2)

    result = _list_workspaces(api_client)
    assert result["customizeEnabled"] is True
    # Synthetic seed must have populated m_workspaces
    assert result["count"] >= 1


# ---------------------------------------------------------------------------
# Workspace CRUD
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_add_workspace_assigns_id_over_999(api_client, clean_state):
    """Workspaces are assigned ids starting at 1000 to avoid group-id clashes."""
    result = _add_workspace(api_client, "My View")
    assert result["id"] >= 1000
    assert result["added"] is True


@pytest.mark.project
def test_add_workspace_auto_enables_customize(api_client, clean_state):
    """Adding a workspace implicitly flips customizeWorkspaces=true."""
    assert _customize_get(api_client)["enabled"] is False

    _add_workspace(api_client, "First")
    time.sleep(0.15)

    assert _customize_get(api_client)["enabled"] is True


@pytest.mark.project
def test_delete_workspace_removes_it(api_client, clean_state):
    """Deleting a workspace removes it from the list."""
    wid = _add_workspace(api_client, "Temp")["id"]
    time.sleep(0.15)

    _delete_workspace(api_client, wid)
    time.sleep(0.15)

    ids = [w["id"] for w in _list_workspaces(api_client)["workspaces"]]
    assert wid not in ids


@pytest.mark.project
def test_rename_workspace_applies(api_client, clean_state):
    """Renaming updates the title in subsequent list/get queries."""
    wid = _add_workspace(api_client, "Original")["id"]
    time.sleep(0.15)

    _rename_workspace(api_client, wid, "Renamed")
    time.sleep(0.15)

    assert _get_workspace(api_client, wid)["title"] == "Renamed"


@pytest.mark.project
def test_rename_workspace_rejects_empty(api_client, clean_state):
    """Empty titles are refused with INVALID_PARAM."""
    wid = _add_workspace(api_client, "X")["id"]
    time.sleep(0.1)

    with pytest.raises(APIError) as ei:
        _rename_workspace(api_client, wid, "   ")
    assert ei.value.code == "INVALID_PARAM"


@pytest.mark.project
def test_get_missing_workspace_errors(api_client, clean_state):
    """Getting a bogus id returns INVALID_PARAM."""
    with pytest.raises(APIError) as ei:
        _get_workspace(api_client, 99999)
    assert ei.value.code == "INVALID_PARAM"


# ---------------------------------------------------------------------------
# Auto-generate
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_auto_generate_materialises_list(api_client, clean_state):
    """With 2+ groups, autoGenerate produces a customised list."""
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "B", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    result = _auto_generate(api_client)
    assert result["generated"] is True
    assert result["firstWorkspaceId"] >= 1000

    # Customize flag must be on now
    assert _customize_get(api_client)["enabled"] is True


@pytest.mark.project
def test_auto_generate_on_empty_project_is_noop(api_client, clean_state):
    """With no eligible groups, autoGenerate returns generated=false."""
    result = _auto_generate(api_client)
    assert result["generated"] is False
    assert result["firstWorkspaceId"] == -1
    assert _customize_get(api_client)["enabled"] is False


@pytest.mark.project
def test_auto_generate_refuses_to_clobber_customised(api_client, clean_state):
    """
    Once the user has a hand-curated customised list, autoGenerate
    must NOT replace it — it guard-returns the first existing id.
    """
    # Seed a customised workspace
    wid = _add_workspace(api_client, "Hand-picked")["id"]
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "B", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    before = _list_workspaces(api_client)["count"]
    result = _auto_generate(api_client)

    # Must NOT have regenerated — first id returned is the existing one
    assert result["firstWorkspaceId"] == wid
    after = _list_workspaces(api_client)["count"]
    assert after == before


# ---------------------------------------------------------------------------
# Widget refs
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_widget_add_and_get(api_client, clean_state):
    """Adding a widget ref appears in the workspace's widgets list."""
    wid = _add_workspace(api_client, "W")["id"]
    time.sleep(0.15)

    _widget_add(api_client, wid, widget_type=3, group_id=0, relative_index=0)
    time.sleep(0.15)

    widgets = _get_workspace(api_client, wid)["widgets"]
    assert len(widgets) == 1
    assert widgets[0]["widgetType"] == 3
    assert widgets[0]["groupId"] == 0
    assert widgets[0]["relativeIndex"] == 0


@pytest.mark.project
def test_widget_remove_takes_triple_key(api_client, clean_state):
    """Widget removal matches the full (type, groupId, relativeIndex) triple."""
    wid = _add_workspace(api_client, "W")["id"]
    time.sleep(0.15)

    _widget_add(api_client, wid, widget_type=3, group_id=0, relative_index=0)
    _widget_add(api_client, wid, widget_type=3, group_id=0, relative_index=1)
    time.sleep(0.15)

    _widget_remove(api_client, wid, widget_type=3, group_id=0, relative_index=0)
    time.sleep(0.15)

    widgets = _get_workspace(api_client, wid)["widgets"]
    assert len(widgets) == 1
    assert widgets[0]["relativeIndex"] == 1


# ---------------------------------------------------------------------------
# REGRESSION — group delete shifts relativeIndex correctly
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_group_delete_shifts_ref_group_id(api_client, clean_state):
    """
    Refs in groups after the deleted one must have their groupId
    decremented by 1 (contiguous renumber).
    """
    _add_group_with_datasets(api_client, "G0", dataset_count=1)
    _add_group_with_datasets(api_client, "G1", dataset_count=1)
    _add_group_with_datasets(api_client, "G2", dataset_count=1)
    time.sleep(0.3)

    wid = _add_workspace(api_client, "W")["id"]
    time.sleep(0.15)

    # Widget ref pointing at G2
    _widget_add(api_client, wid, widget_type=3, group_id=2, relative_index=2)
    time.sleep(0.15)

    # Select + delete G1 — surviving G2 becomes G1
    _select_group(api_client, group_id=1)
    api_client.command("project.group.delete")
    time.sleep(0.25)

    # G2 is now G1; ref groupId must have shifted 2→1
    widgets = _get_workspace(api_client, wid)["widgets"]
    assert len(widgets) == 1
    assert widgets[0]["groupId"] == 1


@pytest.mark.project
def test_group_delete_shifts_ref_relative_index(api_client, clean_state):
    """
    After deleting a group that contributed N same-type widgets, every ref
    in a later group loses N from its relativeIndex.

    We use the DataGrid GroupWidget so each group contributes exactly one
    group-level DashboardDataGrid widget. Enum values:
     * project.group.add widgetType: 0 = GroupWidget::DataGrid
     * workspace widgetType:         1 = DashboardWidget::DashboardDataGrid
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=0)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=0)
    _add_group_with_datasets(api_client, "G2", widget_type=0, dataset_count=0)
    time.sleep(0.3)

    wid = _add_workspace(api_client, "W")["id"]
    time.sleep(0.15)

    # Each group contributes exactly one DashboardDataGrid widget; relative
    # indices across all three groups are 0, 1, 2. Pin a ref to the last one.
    _widget_add(api_client, wid, widget_type=1, group_id=2, relative_index=2)
    time.sleep(0.15)

    _select_group(api_client, group_id=1)
    time.sleep(0.15)
    api_client.command("project.group.delete")
    time.sleep(0.25)

    # G2 became G1; it contributed 1 DataGrid, and the deleted G1 also
    # contributed 1 DataGrid. The ref's relativeIndex 2 → 2 - 1 = 1.
    widgets = _get_workspace(api_client, wid)["widgets"]
    assert len(widgets) == 1
    assert widgets[0]["groupId"] == 1
    assert widgets[0]["relativeIndex"] == 1


@pytest.mark.project
def test_group_delete_drops_refs_pointing_at_deleted(api_client, clean_state):
    """Refs whose groupId equals the deleted group's id are removed."""
    # widgetType 0 = GroupWidget::DataGrid; widget_type=1 in refs = DashboardDataGrid
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=0)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=0)
    time.sleep(0.3)

    wid = _add_workspace(api_client, "W")["id"]
    time.sleep(0.15)

    _widget_add(api_client, wid, widget_type=1, group_id=1, relative_index=1)
    time.sleep(0.15)

    # Delete G1 — ref pointing at it must disappear
    _select_group(api_client, group_id=1)
    time.sleep(0.15)
    api_client.command("project.group.delete")
    time.sleep(0.25)

    widgets = _get_workspace(api_client, wid)["widgets"]
    assert widgets == []


# ---------------------------------------------------------------------------
# REGRESSION — synthetic workspace IDs stay stable across group add/remove
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_synthetic_workspace_ids_stable_across_group_add(api_client, clean_state):
    """
    Synthetic workspace IDs must NOT renumber when group additions flip the
    Overview / All Data conditionals. A persisted selectedWorkspaceId would
    otherwise silently point at the wrong workspace after adding a group.
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    before = _list_workspaces(api_client)["workspaces"]
    before_by_title = {w["title"]: w["id"] for w in before}

    _add_group_with_datasets(api_client, "G2", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    after = _list_workspaces(api_client)["workspaces"]
    after_by_title = {w["title"]: w["id"] for w in after}

    # Every workspace that existed before must have the same id now
    for title, old_id in before_by_title.items():
        assert title in after_by_title, f"Workspace {title!r} disappeared"
        assert after_by_title[title] == old_id, (
            f"Synthetic workspace {title!r} renumbered "
            f"{old_id} → {after_by_title[title]} on group add"
        )


# ---------------------------------------------------------------------------
# REGRESSION -- workspace JSON round-trip
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_save_in_auto_mode_omits_workspace_keys(api_client, clean_state):
    """
    Auto state must NOT serialize customizeWorkspaces or workspaces -- the auto
    layout is derived on load. Older builds reading the file would otherwise
    auto-promote a stale list into customize mode.
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    config = api_client.command("project.exportJson")["config"]
    assert "customizeWorkspaces" not in config, (
        "Auto mode must not emit customizeWorkspaces; older builds would promote it"
    )
    assert "workspaces" not in config, (
        "Auto mode must not emit workspaces array; the layout is derived"
    )


@pytest.mark.project
def test_save_in_customize_mode_emits_both_keys(api_client, clean_state):
    """Customize state must serialize both keys verbatim."""
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.15)

    config = api_client.command("project.exportJson")["config"]
    assert config.get("customizeWorkspaces") is True
    assert isinstance(config.get("workspaces"), list)


@pytest.mark.project
def test_round_trip_auto_state_preserves_flag(api_client, clean_state):
    """
    Save a project in auto mode, reload it, and confirm the customize flag
    stays false. This is the bug that triggered the audit -- before the fix,
    the writer always emitted customizeWorkspaces=false plus a workspaces
    array, and the loader's pre-v3.3 detection would auto-promote it.
    """
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "B", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    config = api_client.command("project.exportJson")["config"]

    api_client.create_new_project(title="Reload Target")
    time.sleep(0.3)
    api_client.command("project.loadFromJSON", {"config": config})
    time.sleep(0.3)

    assert _customize_get(api_client)["enabled"] is False, (
        "Round-trip must not promote auto-saved project to customized"
    )


@pytest.mark.project
def test_round_trip_customized_state_preserves_list(api_client, clean_state):
    """Customized projects round-trip with their workspace list intact."""
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.15)
    custom_id = _add_workspace(api_client, "Hand-picked")["id"]
    time.sleep(0.15)

    config = api_client.command("project.exportJson")["config"]

    api_client.create_new_project(title="Reload Target")
    time.sleep(0.3)
    api_client.command("project.loadFromJSON", {"config": config})
    time.sleep(0.3)

    assert _customize_get(api_client)["enabled"] is True
    ids = [w["id"] for w in _list_workspaces(api_client)["workspaces"]]
    assert custom_id in ids, (
        f"Customized workspace {custom_id} missing after round-trip; got {ids}"
    )


@pytest.mark.project
def test_unflagged_workspaces_array_is_ignored(api_client, clean_state):
    """
    Defensive: a stray workspaces array with no customizeWorkspaces flag should
    NOT promote the project to customize. 3.2.6 had no workspaces feature, so
    any real file in the wild always carries the flag.
    """
    api_client.create_new_project(title="Stray")
    time.sleep(0.3)

    config = api_client.command("project.exportJson")["config"]
    config.pop("customizeWorkspaces", None)
    config["workspaces"] = [{
        "workspaceId": 5000,
        "title": "Stray View",
        "icon": "",
        "widgetRefs": [],
    }]

    api_client.create_new_project(title="Loader")
    time.sleep(0.3)
    api_client.command("project.loadFromJSON", {"config": config})
    time.sleep(0.3)

    assert _customize_get(api_client)["enabled"] is False, (
        "Workspaces array without flag must not auto-promote"
    )


# ---------------------------------------------------------------------------
# REGRESSION -- toggling Customize seeds the editor immediately
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_customize_off_to_on_seeds_immediately(api_client, clean_state):
    """
    The "two clicks" bug: setCustomizeWorkspaces(true) used to leave m_workspaces
    empty if it had been emptied earlier in the session. The fix seeds from
    buildAutoWorkspaces() on every Off->On transition.
    """
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "B", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    # Auto state: workspaces are derived
    auto_count = _list_workspaces(api_client)["count"]
    assert auto_count >= 1, "Two-group project must produce >=1 auto workspace"

    _customize_set(api_client, True)
    time.sleep(0.2)

    # The first click must populate the list, not require a second toggle
    customized = _list_workspaces(api_client)
    assert customized["customizeEnabled"] is True
    assert customized["count"] == auto_count, (
        f"Off->On must seed from auto list (had {auto_count}, now {customized['count']})"
    )


@pytest.mark.project
def test_customize_off_discards_user_edits(api_client, clean_state):
    """
    On->Off is destructive by design -- it drops user customisations and
    rebuilds the synthetic layout. The new resetWorkspacesToAuto helper makes
    this an explicit, named operation.
    """
    _add_group_with_datasets(api_client, "A", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.15)
    user_id = _add_workspace(api_client, "User Workspace")["id"]
    time.sleep(0.15)

    _customize_set(api_client, False)
    time.sleep(0.2)

    ids = [w["id"] for w in _list_workspaces(api_client)["workspaces"]]
    assert user_id not in ids, (
        "Off transition must drop user-added workspaces, not preserve them"
    )


# ---------------------------------------------------------------------------
# REGRESSION -- API guards reject mutations in auto mode
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_widget_add_in_auto_mode_is_rejected(api_client, clean_state):
    """
    The API must refuse widget mutations while customize is off; auto-IDs are
    unstable across structural edits and a soft-promote could land the ref on
    the wrong workspace.
    """
    _add_group_with_datasets(api_client, "G", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    auto = _list_workspaces(api_client)["workspaces"]
    if not auto:
        pytest.skip("Project produced no auto workspaces")
    auto_id = auto[0]["id"]

    with pytest.raises(APIError) as ei:
        _widget_add(api_client, auto_id, widget_type=1, group_id=0, relative_index=0)
    assert ei.value.code == "INVALID_PARAM"
    assert "customize" in str(ei.value).lower()


@pytest.mark.project
def test_workspace_remove_in_auto_mode_is_rejected(api_client, clean_state):
    """API workspace deletion requires customize mode; taskbar uses hideGroup."""
    _add_group_with_datasets(api_client, "G", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    auto = _list_workspaces(api_client)["workspaces"]
    if not auto:
        pytest.skip("Project produced no auto workspaces")
    auto_id = auto[0]["id"]

    with pytest.raises(APIError) as ei:
        _delete_workspace(api_client, auto_id)
    assert ei.value.code == "INVALID_PARAM"


# ---------------------------------------------------------------------------
# REGRESSION -- user IDs do not collide with auto per-group IDs
# ---------------------------------------------------------------------------

@pytest.mark.project
def test_user_workspace_ids_disjoint_from_auto_range(api_client, clean_state):
    """
    Auto IDs use [1000, 5000); user IDs start at 5000. A new user workspace
    must not land on an ID a future group could claim, otherwise adding a
    group later would silently merge the user's workspace with the new group.
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    new_id = _add_workspace(api_client, "User Pick")["id"]
    time.sleep(0.15)

    assert new_id >= 5000, (
        f"User-defined workspace ID {new_id} falls inside the auto reserved "
        "range [1000, 5000); future group additions would collide"
    )


@pytest.mark.project
def test_per_group_workspace_renumbers_on_group_delete(api_client, clean_state):
    """
    Per-group workspaces in customize mode use IDs 1002 + groupId. Deleting
    a group must drop the dead per-group workspace and shift later per-group
    IDs down. Otherwise the workspace named after the deleted group hangs
    around with stale refs.
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G2", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.2)

    before = _list_workspaces(api_client)["workspaces"]
    per_group_before = sorted(w["id"] for w in before
                              if 1002 <= w["id"] < 5000)
    assert per_group_before == [1002, 1003, 1004], (
        f"Expected per-group IDs [1002,1003,1004], got {per_group_before}"
    )

    _select_group(api_client, group_id=1)
    api_client.command("project.group.delete")
    time.sleep(0.3)

    after = _list_workspaces(api_client)["workspaces"]
    per_group_after = sorted(w["id"] for w in after
                             if 1002 <= w["id"] < 5000)
    assert per_group_after == [1002, 1003], (
        f"Expected per-group IDs [1002,1003] after deleting group 1, "
        f"got {per_group_after}"
    )


@pytest.mark.project
def test_user_deleted_auto_workspace_stays_deleted(api_client, clean_state):
    """
    In customize mode, a user-deleted per-group auto workspace must not be
    resurrected by the next groupsChanged tick. The merge logic uses
    m_autoSnapshot to distinguish "never seen" from "explicitly deleted".
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    _add_group_with_datasets(api_client, "G1", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    _customize_set(api_client, True)
    time.sleep(0.2)

    before = _list_workspaces(api_client)["workspaces"]
    per_group_ids = [w["id"] for w in before if w["id"] >= 1002 and w["id"] < 5000]
    if not per_group_ids:
        pytest.skip("No per-group auto workspaces produced for this project")

    target = per_group_ids[0]
    _delete_workspace(api_client, target)
    time.sleep(0.15)

    # Trigger a groupsChanged-like structural edit (rename a group via update path
    # is overkill -- just add another group, which is enough to fire the merge).
    _add_group_with_datasets(api_client, "G2", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    after_ids = [w["id"] for w in _list_workspaces(api_client)["workspaces"]]
    assert target not in after_ids, (
        f"Workspace {target} resurrected after user deleted it"
    )


@pytest.mark.project
def test_adding_group_after_user_workspace_does_not_overwrite(api_client, clean_state):
    """
    Concrete repro of the collision: with the reserved-range fix, adding a
    new group after a user workspace must not change the user workspace's
    title or contents.
    """
    _add_group_with_datasets(api_client, "G0", widget_type=0, dataset_count=1)
    time.sleep(0.3)

    user_wid = _add_workspace(api_client, "Hand-picked")["id"]
    time.sleep(0.15)

    # Add several groups; auto IDs grow but never overlap user_wid (>=5000)
    for i in range(1, 4):
        _add_group_with_datasets(api_client, f"G{i}", widget_type=0, dataset_count=1)
        time.sleep(0.15)

    after = _list_workspaces(api_client)["workspaces"]
    user_ws = next((w for w in after if w["id"] == user_wid), None)
    assert user_ws is not None, "User workspace disappeared after adding groups"
    assert user_ws["title"] == "Hand-picked", (
        f"User workspace title was overwritten: {user_ws['title']!r}"
    )


# ---------------------------------------------------------------------------
# Helpers that use available API commands
# ---------------------------------------------------------------------------

def _select_group(api_client, group_id):
    """Select a group by id so delete/duplicate operations target it."""
    api_client.command("project.group.select", {"groupId": group_id})
    time.sleep(0.1)
