/*
 * Xournal++
 *
 * Chat side panel
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatPanel.h"

#include <algorithm>
#include <thread>

#include <gio/gio.h>
#include <glib.h>

#include "ai/LLMEngine.h"
#include "ai/PDFContextExtractor.h"
#include "chat/ChatInput.h"
#include "chat/ChatMessage.h"
#include "chat/ModelManager.h"
#include "control/Control.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
#include "gui/PdfFloatingToolbox.h"
#include "control/tools/PdfElemSelection.h"
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"
#include "util/Util.h"
#include "util/XojMsgBox.h"
#include "util/gtk4_helper.h"

namespace xoj::chat {

ChatPanel::ChatPanel(Control* control, MainWindow* window): control(control), window(window) {
    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_vexpand(root, true);
    gtk_widget_set_hexpand(root, true);
    gtk_widget_set_size_request(root, 200, -1);
    gtk_widget_add_css_class(root, "chat-panel");

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "chat-header");

    GtkWidget* title = gtk_label_new("Assistente");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, true);

    contextButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(contextButton), "document-open");
    gtk_widget_set_tooltip_text(contextButton, "Context");
    gtk_widget_set_can_focus(contextButton, false);

    clearButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(clearButton), "edit-clear");
    gtk_widget_set_tooltip_text(clearButton, "Clear conversation");
    gtk_widget_set_can_focus(clearButton, false);

    closeButton = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(closeButton), "window-close");
    gtk_widget_set_tooltip_text(closeButton, "Close chat");
    gtk_widget_set_can_focus(closeButton, false);

    copilotLoginButton = gtk_button_new_with_label("Autenticar Copilot");
    gtk_widget_set_tooltip_text(copilotLoginButton, "Login na conta GitHub para usar o modelo GitHub Copilot");
    gtk_widget_set_can_focus(copilotLoginButton, false);

    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), contextButton);
    gtk_box_append(GTK_BOX(header), copilotLoginButton);
    gtk_box_append(GTK_BOX(header), clearButton);
    gtk_box_append(GTK_BOX(header), closeButton);

    gtk_box_append(GTK_BOX(root), header);

    GtkWidget* settingsRow = gtk_flow_box_new();
    gtk_widget_add_css_class(settingsRow, "chat-settings");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(settingsRow), GTK_SELECTION_NONE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(settingsRow), 4);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(settingsRow), 4);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(settingsRow), 6);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(settingsRow), 1);

    GtkWidget* modelLabel = gtk_label_new("Modelo");
    gtk_widget_set_halign(modelLabel, GTK_ALIGN_START);
    modelCombo = gtk_combo_box_text_new();
    gtk_widget_set_size_request(modelCombo, 130, -1);
    gtk_widget_set_hexpand(modelCombo, true);

    GtkWidget* contextLabel = gtk_label_new("Ctx");
    gtk_widget_set_halign(contextLabel, GTK_ALIGN_START);
    contextCombo = gtk_combo_box_text_new();
    gtk_widget_set_size_request(contextCombo, 100, -1);
    gtk_widget_set_hexpand(contextCombo, true);

    GtkWidget* contextSizeLabel = gtk_label_new("Máx");
    gtk_widget_set_halign(contextSizeLabel, GTK_ALIGN_START);
    contextSizeSpin = gtk_spin_button_new_with_range(1000, 50000, 1000);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(contextSizeSpin), 0);
    gtk_widget_set_size_request(contextSizeSpin, 70, -1);
    gtk_widget_set_hexpand(contextSizeSpin, true);

    useGhCheck = gtk_check_button_new_with_label("Usar conta GitHub (gh)");
    gtk_widget_set_tooltip_text(useGhCheck,
                                "Descarregar modelos de releases do GitHub com a conta autenticada (gh auth login)");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(useGhCheck),
                                 control->getSettings()->getUseGhForModelDownload());

    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), modelLabel, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), modelCombo, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), contextLabel, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), contextCombo, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), contextSizeLabel, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), contextSizeSpin, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(settingsRow), useGhCheck, -1);
    gtk_box_append(GTK_BOX(root), settingsRow);

    g_signal_connect(useGhCheck, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (self && self->control) {
                             self->control->getSettings()->setUseGhForModelDownload(
                                     gtk_check_button_get_active(GTK_CHECK_BUTTON(btn)));
                         }
                     }),
                     this);

    g_signal_connect(copilotLoginButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (self) self->onCopilotLoginClicked();
                     }),
                     this);

    scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, true);
    gtk_widget_set_hexpand(scroller, true);

    listBox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox), GTK_SELECTION_NONE);
    gtk_widget_set_hexpand(listBox, true);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), listBox);
    gtk_box_append(GTK_BOX(root), scroller);

    input = std::make_unique<ChatInput>();
    gtk_box_append(GTK_BOX(root), input->getWidget());

    gtk_popover_set_relative_to(GTK_POPOVER(contextSelector.getWidget()), contextButton);

    refreshModelChoices();

    auto& latexSettings = control->getSettings()->latexSettings;
    fs::path templatePath = latexSettings.globalTemplatePath;
    if (!templatePath.empty()) {
        if (auto templateText = Util::readString(templatePath, false, std::ios::binary)) {
            latexRenderer.configure(latexSettings, std::move(*templateText));
        }
    }
    fs::path latex2Svg = Util::getLatex2SvgPath();
    if (!latex2Svg.empty() && fs::exists(latex2Svg)) {
        latexRenderer.setLatex2SvgPath(std::move(latex2Svg));
    }

    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(contextCombo), "current_page", "Página atual");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(contextCombo), "selection", "Texto selecionado");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(contextCombo), "document", "Documento inteiro");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(contextCombo), "none", "Sem contexto");

    const std::string& savedContext = control->getSettings()->getChatContext();
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(contextCombo), savedContext.c_str());
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(contextCombo)) < 0) {
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(contextCombo), "current_page");
    }

    int savedContextSize = control->getSettings()->getChatContextSize();
    if (savedContextSize <= 0) {
        savedContextSize = 12000;
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(contextSizeSpin), savedContextSize);

    g_signal_connect(contextCombo, "changed", G_CALLBACK(+[](GtkComboBox* combo, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (!self) {
                             return;
                         }
                         const char* id = gtk_combo_box_get_active_id(combo);
                         if (id) {
                             self->control->getSettings()->setChatContext(id);
                         }
                     }),
                     this);

    g_signal_connect(contextSizeSpin, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (!self) {
                             return;
                         }
                         int value = static_cast<int>(gtk_spin_button_get_value(spin));
                         self->control->getSettings()->setChatContextSize(value);
                     }),
                     this);

    g_signal_connect(contextButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (!self) {
                             return;
                         }
                         Document* doc = self->control->getDocument();
                         doc->lock();
                         int pageCount = static_cast<int>(doc->getPageCount());
                         doc->unlock();
                         self->contextSelector.setRangeLimits(1, std::max(1, pageCount));
                         gtk_popover_popup(GTK_POPOVER(self->contextSelector.getWidget()));
                     }),
                     this);

    g_signal_connect(clearButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (self) {
                             self->clear();
                         }
                     }),
                     this);

    g_signal_connect(closeButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (self) {
                             self->window->setChatVisible(false);
                         }
                     }),
                     this);

    input->setSendCallback([this]() { sendMessage(); });
    input->setCancelCallback([this]() { cancelGeneration(); });
}

GtkWidget* ChatPanel::getWidget() const { return root; }

void ChatPanel::focusInput() { input->focus(); }

void ChatPanel::clear() {
    GList* children = gtk_container_get_children(GTK_CONTAINER(listBox));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
}

void ChatPanel::addMessage(Role role, const std::string& text) {
    ChatMessage message(role, text, &latexRenderer);
    GtkWidget* row = message.buildWidget();
    gtk_list_box_insert(GTK_LIST_BOX(listBox), row, -1);
    gtk_widget_show_all(row);
}

void ChatPanel::addSystemMessage(const std::string& text) { addMessage(Role::SYSTEM, text); }

std::string ChatPanel::buildContext(const std::string& contextId) const {
    Document* doc = control->getDocument();
    if (!doc) {
        return {};
    }

    int maxContext = control->getSettings()->getChatContextSize();
    if (maxContext <= 0) {
        maxContext = 12000;
    }

    std::string context;
    doc->lock();
    size_t pageCount = doc->getPageCount();
    doc->unlock();

    std::string selectedText;
    if (contextId == "selection") {
        if (auto* toolbox = window->getPdfToolbox(); toolbox && toolbox->hasSelection()) {
            if (auto* pdfSelection = toolbox->getSelection()) {
                selectedText = pdfSelection->getSelectedText();
            }
        }
    }

    if (contextId == "current_page") {
        size_t pageNo = control->getCurrentPageNo();
        context += PDFContextExtractor::extract(doc, pageNo, "");
    } else if (contextId == "selection") {
        context += PDFContextExtractor::extract(doc, control->getCurrentPageNo(), selectedText);
    } else if (contextId == "document") {
        for (size_t i = 0; i < pageCount; ++i) {
            if (!context.empty()) {
                context += "\n\n";
            }
            context += PDFContextExtractor::extract(doc, i, "");
            if (context.size() > static_cast<size_t>(maxContext)) {
                context += "\n...";
                break;
            }
        }
    }

    if (context.size() > static_cast<size_t>(maxContext)) {
        context.resize(static_cast<size_t>(maxContext));
        context += "\n...";
    }

    return context;
}

void ChatPanel::sendMessage() {
    std::string question = StringUtils::trim(input->getText());
    if (question.empty()) {
        addSystemMessage("Type a question to continue.");
        return;
    }

    addMessage(Role::USER, question);
    input->clear();
    input->setEnabled(false);
    cancelRequested.store(false);

    addSystemMessage("Thinking...");

    std::string contextId = getSelectedContextId();
    std::string context = buildContext(contextId);
    std::string modelId = getSelectedModelId();

    if (modelId.empty()) {
        control->ensureLLMModel([this, question, context](bool ok, const std::string& modelPath) {
            if (!ok) {
                addSystemMessage(modelPath.empty() ? "Model unavailable." : modelPath);
                input->setEnabled(true);
                return;
            }
            runModelOrCopilot(modelPath, question, context);
        });
        return;
    }

    control->ensureLLMModel(modelId, [this, question, context](bool ok, const std::string& modelPath) {
        if (!ok) {
            addSystemMessage(modelPath.empty() ? "Model unavailable." : modelPath);
            input->setEnabled(true);
            return;
        }
        runModelOrCopilot(modelPath, question, context);
    });
}

void ChatPanel::runModelOrCopilot(const std::string& modelPath, const std::string& question,
                                  const std::string& context) {
    std::thread([this, modelPath, question, context]() {
        std::string prompt = "Você é um assistente de matemática de nível universitário.\n"
                             "Responda em português (pt-BR).\n"
                             "Use LaTeX para fórmulas.\n"
                             "Responda usando apenas o contexto fornecido.\n\nContexto:\n" +
                             context + "\n\nPergunta:\n" + question + "\n";

        std::string response;
        if (modelPath == "copilot") {
            std::string copilotPathStr;
            {
                fs::path bundled = Util::getBundledCopilotPath();
                if (!bundled.empty() && fs::exists(bundled) && fs::is_regular_file(bundled)) {
                    copilotPathStr = bundled.string();
                }
                if (copilotPathStr.empty()) {
                    char* found = g_find_program_in_path("copilot");
                    if (found) {
                        copilotPathStr = found;
                        g_free(found);
                    }
                }
            }
            if (copilotPathStr.empty()) {
                response =
                        "GitHub Copilot CLI not found. Use \"Autenticar Copilot\" no painel ou instale: brew install "
                        "copilot-cli (ou npm install -g @github/copilot), depois faça login.";
            } else {
                GError* err = nullptr;
                GSubprocess* proc = g_subprocess_new(
                        static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                      G_SUBPROCESS_FLAGS_STDERR_PIPE),
                        &err, copilotPathStr.c_str(), "-p", prompt.c_str(), "-s", "--allow-all", nullptr);
                if (!proc) {
                    response = err ? err->message : "Failed to start Copilot CLI.";
                    if (err) g_error_free(err);
                } else {
                    GInputStream* outStream = g_subprocess_get_stdout_pipe(proc);
                    std::string outStr;
                    char buf[4096];
                    while (!cancelRequested.load()) {
                        GError* readErr = nullptr;
                        gssize n = g_input_stream_read(outStream, buf, sizeof(buf), nullptr, &readErr);
                        if (n <= 0) {
                            if (readErr) g_error_free(readErr);
                            break;
                        }
                        outStr.append(buf, static_cast<size_t>(n));
                    }
                    g_subprocess_force_exit(proc);
                    g_subprocess_wait(proc, nullptr, nullptr);
                    g_object_unref(proc);
                    response = StringUtils::trim(outStr);
                    if (response.empty()) {
                        response = "Copilot returned no text. Check 'copilot login' and subscription.";
                    }
                }
            }
        } else {
            LLMEngine engine;
            if (!engine.init(modelPath)) {
                response = "Failed to load model.";
            } else {
                response = engine.run(prompt);
                engine.shutdown();
            }
        }

        if (cancelRequested.load()) {
            return;
        }

        Util::execInUiThread([this, response]() {
            addMessage(Role::ASSISTANT, response.empty() ? "No response." : response);
            input->setEnabled(true);
        });
    }).detach();
}

std::string ChatPanel::getSelectedModelId() const {
    const char* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(modelCombo));
    if (id) {
        if (std::string(id) == "external") {
            return "";
        }
        return id;
    }
    return "phi3-mini-math";
}

std::string ChatPanel::getSelectedContextId() const {
    const char* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(contextCombo));
    if (id) {
        return id;
    }
    return "current_page";
}

void ChatPanel::refreshModelChoices() {
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(modelCombo));

    const char* envModel = g_getenv("XOURNALPP_LLM_MODEL");
    if (envModel && *envModel) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo), "external", "Modelo externo (env)");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo), "external");
        gtk_widget_set_sensitive(modelCombo, false);
        return;
    }

    for (const auto& model: ModelManager::listModels()) {
        std::string label = model.name;
        if (!ModelManager::isInstalled(model)) {
            label += " (download)";
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo), model.id.c_str(), label.c_str());
    }

    const std::string& saved = control->getSettings()->getChatModel();
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo), saved.c_str());
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(modelCombo)) < 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(modelCombo), 0);
    }

    g_signal_connect(modelCombo, "changed", G_CALLBACK(+[](GtkComboBox* combo, gpointer userData) {
                         auto* self = static_cast<ChatPanel*>(userData);
                         if (!self) {
                             return;
                         }
                         const char* id = gtk_combo_box_get_active_id(combo);
                         if (id) {
                             self->control->getSettings()->setChatModel(id);
                         }
                     }),
                     this);
}

void ChatPanel::cancelGeneration() {
    cancelRequested.store(true);
    addSystemMessage("Generation cancelled.");
    input->setEnabled(true);
}

std::string ChatPanel::getCopilotPath() {
    fs::path bundled = Util::getBundledCopilotPath();
    if (!bundled.empty() && fs::exists(bundled) && fs::is_regular_file(bundled)) {
        return bundled.string();
    }
    char* found = g_find_program_in_path("copilot");
    if (found) {
        std::string s(found);
        g_free(found);
        return s;
    }
    return {};
}

struct CopilotLoginData {
    GtkWidget* dialog = nullptr;
    GtkWidget* outputView = nullptr;  // GtkTextView to show stdout/stderr (incl. code)
    GtkWidget* button = nullptr;
    GSubprocess* process = nullptr;
    std::atomic<bool> cancelled{false};
};

static void copilot_login_append_output(CopilotLoginData* data, const std::string& text) {
    if (!data->outputView) return;
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->outputView));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, text.c_str(), -1);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(data->outputView), &end, 0.0, FALSE, 0.0, 0.0);
}

static void copilot_login_data_free(gpointer p) {
    auto* data = static_cast<CopilotLoginData*>(p);
    if (data->process) {
        g_subprocess_force_exit(data->process);
        g_object_unref(data->process);
    }
    delete data;
}

static void on_copilot_login_cancel_clicked(GtkButton* btn, gpointer userData) {
    auto* data = static_cast<CopilotLoginData*>(userData);
    data->cancelled.store(true);
    if (data->process) {
        g_subprocess_force_exit(data->process);
    }
    copilot_login_append_output(data, "\n\nCancelado. Pode fechar a janela.");
    gtk_button_set_label(GTK_BUTTON(data->button), "Fechar");
    g_signal_handlers_disconnect_by_data(data->button, data);
    g_signal_connect_swapped(data->button, "clicked", G_CALLBACK(gtk_widget_destroy), data->dialog);
}

static gboolean copilot_login_finished_idle(gpointer userData) {
    auto* data = static_cast<CopilotLoginData*>(userData);
    if (data->process) {
        g_object_unref(data->process);
        data->process = nullptr;
    }
    if (!data->dialog || !gtk_widget_get_visible(data->dialog)) {
        return G_SOURCE_REMOVE;
    }
    copilot_login_append_output(data,
                                data->cancelled.load() ? "\n\nCancelado. Pode fechar a janela."
                                                       : "\n\nLogin concluído. Pode fechar a janela.");
    gtk_button_set_label(GTK_BUTTON(data->button), "Fechar");
    g_signal_handlers_disconnect_by_data(data->button, data);
    g_signal_connect_swapped(data->button, "clicked", G_CALLBACK(gtk_widget_destroy), data->dialog);
    return G_SOURCE_REMOVE;
}

void ChatPanel::onCopilotLoginClicked() {
    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(control->getGtkWindow())));
    if (!parent) {
        parent = GTK_WINDOW(control->getGtkWindow());
    }

    std::string copilotPath = getCopilotPath();
    if (copilotPath.empty()) {
        GtkWidget* dialog = gtk_dialog_new_with_buttons("Login GitHub Copilot", parent, GTK_DIALOG_MODAL, "Fechar",
                                                        GTK_RESPONSE_CLOSE, nullptr);
        GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget* label = gtk_label_new(
                "Copilot CLI não encontrado.\n\n"
                "Para usar o GitHub Copilot:\n"
                "• Inclua o Copilot no pacote da aplicação (build com bundle-copilot.sh), ou\n"
                "• Instale no sistema: brew install copilot-cli (ou npm install -g @github/copilot)\n\n"
                "Depois de instalar, clique novamente em \"Autenticar Copilot\" para fazer login.");
        gtk_label_set_wrap(GTK_LABEL(label), true);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 55);
        gtk_label_set_selectable(GTK_LABEL(label), true);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(content), label);
        gtk_widget_show_all(dialog);
        g_signal_connect_swapped(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE),
                                "clicked", G_CALLBACK(gtk_widget_destroy), dialog);
        return;
    }

    GtkWidget* dialog = gtk_dialog_new_with_buttons("Login GitHub Copilot", parent, GTK_DIALOG_MODAL, "Cancelar",
                                                    GTK_RESPONSE_CANCEL, nullptr);
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* label = gtk_label_new(
            "Será aberta uma janela do browser. Conclua o login lá; o código aparecerá abaixo.");
    gtk_label_set_wrap(GTK_LABEL(label), true);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), label);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroller), 120);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroller), true);
    GtkWidget* outputView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(outputView), false);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(outputView), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(outputView), true);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(outputView), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(outputView), 6);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), outputView);
    gtk_box_append(GTK_BOX(content), scroller);

    GtkWidget* cancelButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

    auto* data = new CopilotLoginData();
    data->dialog = dialog;
    data->outputView = outputView;
    data->button = cancelButton;
    g_object_set_data_full(G_OBJECT(dialog), "copilot-login-data", data, copilot_login_data_free);

    g_signal_connect(cancelButton, "clicked", G_CALLBACK(on_copilot_login_cancel_clicked), data);

    gtk_widget_show_all(dialog);

    std::string path = copilotPath;
    std::thread([path, data]() {
        GError* err = nullptr;
        GSubprocess* proc = g_subprocess_new(
                static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDERR_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE),
                &err, path.c_str(), "login", nullptr);
        if (!proc) {
            std::string msg = err ? err->message : "Falha ao iniciar Copilot.";
            if (err) g_error_free(err);
            Util::execInUiThread([data, msg]() {
                copilot_login_append_output(data, msg);
                gtk_button_set_label(GTK_BUTTON(data->button), "Fechar");
                g_signal_handlers_disconnect_by_data(data->button, data);
                g_signal_connect_swapped(data->button, "clicked", G_CALLBACK(gtk_widget_destroy), data->dialog);
            });
            return;
        }
        data->process = proc;
        GInputStream* outStream = g_subprocess_get_stdout_pipe(proc);
        GInputStream* errStream = g_subprocess_get_stderr_pipe(proc);
        auto read_stream = [data](GInputStream* stream) {
            char buf[512];
            while (!data->cancelled.load()) {
                GError* readErr = nullptr;
                gssize n = g_input_stream_read(stream, buf, sizeof(buf), nullptr, &readErr);
                if (n <= 0) {
                    if (readErr) g_error_free(readErr);
                    break;
                }
                std::string chunk(buf, static_cast<size_t>(n));
                Util::execInUiThread([data, chunk]() { copilot_login_append_output(data, chunk); });
            }
        };
        std::thread outThread([outStream, read_stream]() { read_stream(outStream); });
        std::thread errThread([errStream, read_stream]() { read_stream(errStream); });
        outThread.join();
        errThread.join();
        g_subprocess_wait(proc, nullptr, nullptr);
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, copilot_login_finished_idle, data, nullptr);
    }).detach();
}

}  // namespace xoj::chat
