/* JSON exporter — round-trippable spec for the editor. */

export function exportJson(state) {
  const out = {
    schema: "flipper-gui/v1",
    v: state.v,
    app: state.app,
    screens: state.screens.map((s) => ({
      id: s.id,
      name: s.name,
      widgets: s.widgets,
    })),
    icons: state.icons,
  };
  return {
    filename: `${state.app.namespace || "flipper_gui"}.flipper-gui.json`,
    text: JSON.stringify(out, null, 2),
  };
}
