<?php
// $Id: sms_devel.send_form.inc,v 1.1.2.1 2010/06/25 17:32:00 aspope Exp $
/**
 * @file
 * Test send form include for the sms_devel module of the SMS Framework.
 *
 * @package sms
 * @subpackage sms_devel
 */


/**
 * Test send form
 *
 * @return
 *   Drupal form array.
 *
 * @ingroup forms
 */
function sms_devel_send_form(&$form_state) {
  // Begin my form
  $form['about'] = array(
    '#type'  => 'item',
    '#value' => 'This is a basic form that contains:<ul><li>include sms_send_form()</li><li>message text field</li><li>submit button</li></ul>The form validation includes sms_send_form_validate().<br/>The form submission includes sms_send_form_submit() which sends the message, and a little note that the form submitted ok.',
  );

  // Include the sms_send_form from the SMS Framework core
  $form = array_merge($form, sms_send_form());

  // Message text field for my form
  $form['message'] = array(
    '#type'  => 'textfield',
    '#title' => t('Message'),
    '#size'      => 80,
    '#maxlength' => 160,  // This can be longer if you allow long messages
  );

  // Submit button for my form
  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => 'Submit',
    '#validate' => array('sms_send_form_validate'),
    '#submit'   => array('sms_send_form_submit', 'sms_devel_send_form_submit'),
  );

  return $form;
}

function sms_devel_send_form_submit($form, &$form_state) {
  // These is what happens after the SMS has been sent
  $number  = $form_state['values']['number'];
  $message = $form_state['values']['message'];
  drupal_set_message("Form submitted ok for number $number and message: $message");
}
